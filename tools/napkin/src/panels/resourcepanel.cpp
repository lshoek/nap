/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "resourcepanel.h"
#include "naputils.h"

#include <commands.h>
#include <napqt/filterpopup.h>
#include <napqt/qtutils.h>
#include <napkin-resources.h>
#include <QKeyEvent>

using namespace napkin;

static bool ResourceSorter(const QModelIndex& left, const QModelIndex& right, QAbstractItemModel* model)
{
	// Get model
	assert(qobject_cast<ResourceModel*>(model) != nullptr);
	ResourceModel* resource_model = static_cast<ResourceModel*>(model);

	// Get and cast to RTTI Item
	auto l_item = dynamic_cast<RTTIItem*>(resource_model->itemFromIndex(left));
	auto r_item = dynamic_cast<RTTIItem*>(resource_model->itemFromIndex(right));

	// Bail if we're not an rtti item
	if (l_item == nullptr || r_item == nullptr)
		return false;

	// Don't sort regular resource groups
	if (qobject_cast<EntityResourcesItem*>(l_item) != nullptr &&
		qobject_cast<RootResourcesItem*>(r_item) != nullptr)
		return false;

	// Check if item is an entity
	auto le_item = qobject_cast<EntityItem*>(l_item);
	auto re_item = qobject_cast<EntityItem*>(r_item);

	// Check if item is a component
	auto lc_item = qobject_cast<ComponentItem*>(l_item);
	auto rc_item = qobject_cast<ComponentItem*>(r_item);

	// left is entity, right is component
	if (le_item != nullptr && rc_item != nullptr)
		return true;

	// right is component, left is entity
	if (lc_item != nullptr && re_item != nullptr)
		return false;

	// Don't sort items of which parent is an entity
	if (qobject_cast<EntityItem*>(l_item->parentItem()) != nullptr &&
		qobject_cast<EntityItem*>(r_item->parentItem()) != nullptr)
		return false;

	// Prioritize groups over other items
	GroupItem* lg_item = qobject_cast<GroupItem*>(l_item);
	GroupItem* rg_item = qobject_cast<GroupItem*>(r_item);

	// Left is group, right is not
	if (lg_item != nullptr && rg_item == nullptr)
		return true;

	// Right is group, left is not
	if (rg_item != nullptr && lg_item == nullptr)
		return false;

	// Otherwise sort default
    return l_item->text() < r_item->text();
}


napkin::ResourceModel::ResourceModel()
{
	setHorizontalHeaderLabels({TXT_LABEL_NAME, TXT_LABEL_TYPE});
	appendRow(&mObjectsItem);
	appendRow(&mEntitiesItem);
	setItemPrototype(new RTTIItem());
}


napkin::ResourcePanel::ResourcePanel()
{
	setLayout(&mLayout);
	layout()->setContentsMargins(0, 0, 0, 0);
	mLayout.addWidget(&mTreeView);
	mTreeView.setModel(&mModel);
	mTreeView.getTreeView().setColumnWidth(0, 300);
	mTreeView.enableSorting(&ResourceSorter);

	connect(mTreeView.getSelectionModel(), &QItemSelectionModel::selectionChanged, this, &ResourcePanel::onSelectionChanged);
	mTreeView.setMenuHook(std::bind(&ResourcePanel::menuHook, this, std::placeholders::_1));

	connect(&AppContext::get(), &AppContext::documentOpened, this, &ResourcePanel::onFileOpened);
	connect(&AppContext::get(), &AppContext::documentClosing, this, &ResourcePanel::onFileClosing);
	connect(&AppContext::get(), &AppContext::newDocumentCreated, this, &ResourcePanel::onNewFile);

	auto& resources_item = mModel.getRootResourcesItem();
	resources_item.setEnabled(AppContext::get().getProjectLoaded());
	connect(&resources_item, &RootResourcesItem::childAddedToGroup, this, &ResourcePanel::onChildAddedToGroup);

	auto& entities_item = mModel.getEntityResourcesItem();
	entities_item.setEnabled(AppContext::get().getProjectLoaded());
	connect(&entities_item, &EntityResourcesItem::childAddedToEntity, this, &ResourcePanel::onChildAddedToEntity);
	connect(&AppContext::get(), &AppContext::projectLoaded, this, &ResourcePanel::onProjectLoaded);

	mTreeView.installEventFilter(this);
}


void napkin::ResourceModel::populate()
{
	auto doc = AppContext::get().getDocument();
	assert(doc != nullptr);
	auto root_objects = topLevelObjects();
	mObjectsItem.populate(root_objects);
	mEntitiesItem.populate(root_objects);
}


void napkin::ResourceModel::clearItems()
{
	mEntitiesItem.clear();
	mObjectsItem.clear();
}


/**
 * @return items group, nullptr if there is no parent or the parent isn't a group
 */
static nap::IGroup* getItemGroup(const ObjectItem& item)
{
	// Check if the parent is a group
	GroupItem* parent_item = item.parentItem() != nullptr ?
		qobject_cast<GroupItem*>(item.parentItem()) :
		nullptr;

	// Return group from parent, nullptr otherwise
	return parent_item != nullptr ? &parent_item->getGroup() : nullptr;
}


void napkin::ResourcePanel::menuHook(QMenu& menu)
{
	// Get selection
	auto selected_item = qitem_cast<RTTIItem*>(mTreeView.getSelectedItem());
	if (selected_item == nullptr)
		return;

	// Cast to rtti item
	if (qobject_cast<EntityItem*>(selected_item) != nullptr)
	{
		// If it's a child of another entity
		auto entity_item = static_cast<EntityItem*>(selected_item);
		if (entity_item->isPointer())
		{
			auto parent_item = qobject_cast<EntityItem*>(entity_item->parentItem());
			if (parent_item)
			{
				menu.addAction(new RemovePathAction(&menu, entity_item->propertyPath()));
			}
		}
		// Otherwise it's the main resource
		else
		{
			menu.addAction(new AddChildEntityAction(&menu, entity_item->getEntity()));
			menu.addAction(new AddComponentAction(&menu, entity_item->getEntity()));
			menu.addAction(new DeleteObjectAction(&menu, entity_item->getObject()));
		}
	}
	// Component
	else if (qobject_cast<ComponentItem*>(selected_item) != nullptr)
	{
		auto component_item = static_cast<ComponentItem*>(selected_item);
		menu.addAction(new DeleteObjectAction(&menu, component_item->getObject()));
	}
	// Group
	else if (qobject_cast<GroupItem*>(selected_item) != nullptr)
	{
		// Create and add new resource
		GroupItem* group_item = static_cast<GroupItem*>(selected_item);
		menu.addAction(new AddNewResourceToGroupAction(&menu, group_item->getGroup()));

		// Add existing resource
		menu.addAction(new AddExistingResourceToGroupAction(&menu, group_item->getGroup()));

		// If the item is parented under a group, offer the option to remove it
		auto* item_group = getItemGroup(*group_item);
		if (item_group != nullptr)
			menu.addAction(new RemoveGroupFromGroupAction(&menu, *item_group, group_item->getGroup()));

		// Create and add new sub group
		menu.addAction(new AddChildGroupAction(&menu, group_item->getGroup()));

		// Add action to move group to another group
		menu.addAction(new MoveGroupAction(&menu, group_item->getGroup(), item_group));

		// Delete group action
		menu.addAction(new DeleteGroupAction(&menu, group_item->getGroup()));
	}
	// General Object
	else if (qobject_cast<ObjectItem*>(selected_item) != nullptr)
	{
		// Get resource
		auto object_item = static_cast<ObjectItem*>(selected_item);

		// If the item is parented under a group, offer the option to remove it
		auto* item_group = getItemGroup(*object_item);
		if (item_group != nullptr)
			menu.addAction(new RemoveResourceFromGroupAction(&menu, *item_group, object_item->getObject()));

		// Move resource to another group
		menu.addAction(new MoveResourceToGroupAction(&menu, object_item->getObject(), item_group));

		// Delete resource action
		menu.addAction(new DeleteObjectAction(&menu, object_item->getObject()));

		// If the item is a shader, allow if to be loaded (compiled)
		// TODO: Create for more generic insertion method for object specific actions
		if (object_item->getObject().get_type().is_derived_from(RTTI_OF(nap::BaseShader)))
		{
			menu.addAction(new LoadShaderAction(&menu, static_cast<nap::BaseShader&>(object_item->getObject())));
		}
	}
	// Top Resource
	else if (qobject_cast<RootResourcesItem*>(selected_item) != nullptr)
	{
		// Add Resource selection
		menu.addAction(new CreateResourceAction(&menu));

		// Add groups
		menu.addAction(new CreateGroupAction(&menu));
	}
	// Top Entity
	else if (qobject_cast<EntityResourcesItem*>(selected_item) != nullptr)
	{
		menu.addAction(new CreateEntityAction(&menu));
	}
}


void napkin::ResourcePanel::onProjectLoaded(const nap::ProjectInfo& projectInfo)
{
	mModel.getRootResourcesItem().setEnabled(true);
	mModel.getEntityResourcesItem().setEnabled(true);
}


void napkin::ResourcePanel::onNewFile()
{
	populate();
}


void napkin::ResourcePanel::onFileOpened(const QString& filename)
{
	populate();
}


void napkin::ResourcePanel::onFileClosing(const QString& filename)
{
	clear();
}


void napkin::ResourcePanel::onSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
	emitSelectionChanged();
}


bool napkin::ResourcePanel::eventFilter(QObject* obj, QEvent* ev)
{
	if (obj == &mTreeView && ev->type() == QEvent::KeyPress)
	{
		// Handle deletion of object
		QKeyEvent* key_event = static_cast<QKeyEvent*>(ev);
		if (key_event->key() == Qt::Key_Delete)
		{
			// Cast to 
			auto obj_item = qitem_cast<ObjectItem*>(mTreeView.getSelectedItem());
			if (obj_item != nullptr)
			{
				DeleteObjectAction action(nullptr, obj_item->getObject());
				action.trigger();
			}
			return true;
		}
	}
	return QWidget::eventFilter(obj, ev);
}


void napkin::ResourcePanel::clear()
{
	mTreeView.getTreeView().selectionModel()->clear();
	mModel.clearItems();
}


void napkin::ResourcePanel::populate()
{
	mModel.populate();
	mTreeView.expand(mModel.getRootResourcesItem());
	mTreeView.expand(mModel.getEntityResourcesItem());
	emitSelectionChanged();
	mTreeView.getTreeView().sortByColumn(0, Qt::SortOrder::AscendingOrder);
}


void ResourcePanel::selectObjects(const QList<nap::rtti::Object*>& obj)
{
	if (obj.size() > 0)
		mTreeView.select(findItemInModel<napkin::ObjectItem>(mModel, *obj[0]), true);
}


void napkin::ResourcePanel::onChildAddedToGroup(GroupItem& group, ObjectItem& item)
{
	mTreeView.select(&item, true);
}


void napkin::ResourcePanel::onChildAddedToEntity(EntityItem& entity, ObjectItem& item)
{
	mTreeView.select(&item, true);
}


void ResourcePanel::emitSelectionChanged()
{
	// Grab selected nap objects
	QList<PropertyPath> selectedPaths;
	for (auto m : mTreeView.getSelectedItems())
	{
		auto item = qitem_cast<ObjectItem*>(m);
		if (item != nullptr)
			selectedPaths << item->propertyPath();
	}
	selectionChanged(selectedPaths);
}

