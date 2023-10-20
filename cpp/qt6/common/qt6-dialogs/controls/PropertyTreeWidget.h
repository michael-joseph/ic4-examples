// added { outline: none; } to QTreeView::item in QSS, to remove dottet line in WIN32

#pragma once

#include "PropertyControls.h"
#include "PropertyInfoBox.h"

#include <ic4/ic4.h>

#include <QString>
#include <QAbstractItemModel>
#include <QStyledItemDelegate>
#include <QSortFilterProxyModel>
#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QTreeView>
#include <QHeaderView>
#include <QSplitter>
#include <QPainter>


#include <QTreeWidget>
#include <QPushButton>
#include <QLabel>

#include <vector>
#include <memory>

namespace ic4::ui
{
	struct PropertyTreeNode
	{
		PropertyTreeNode* parent;
		ic4::Property prop;
		int row;

		QString prop_name;
		QString display_name;
		std::vector<std::unique_ptr<PropertyTreeNode>> children;


		ic4::Property::NotificationToken notification_token = {};
		bool prev_available;

		PropertyTreeNode(PropertyTreeNode* parent_, const ic4::Property& prop_, int row_, QString pn, QString dn)
			: parent(parent_)
			, prop(prop_)
			, row(row_)
			, prop_name(std::move(pn))
			, display_name(std::move(dn))
		{
		}

		~PropertyTreeNode()
		{
			if (notification_token)
				prop.eventRemoveNotification(notification_token);
		}

		void populate()
		{
			if (!children.empty())
				return;

			if (prop.getType() == ic4::PropType::Category)
			{
				int index = 0;
				for (auto&& feature : prop.asCategory().getFeatures())
				{
					auto child_name = QString::fromStdString(feature.getName());
					auto child_display_name = QString::fromStdString(feature.getDisplayName());

					switch (feature.getType())
					{
						// only show valid properties
					case ic4::PropType::Integer:
					case ic4::PropType::Command:
					case ic4::PropType::String:
					case ic4::PropType::Enumeration:
					case ic4::PropType::Boolean:
					case ic4::PropType::Float:
						children.push_back(std::make_unique<PropertyTreeNode>(this, feature, index++, child_name, child_display_name));
						break;
					case ic4::PropType::Category:
						// only add category note, if not empty
						if (category_is_empty(feature))
						{
							children.push_back(std::make_unique<PropertyTreeNode>(this, feature, index++, child_name, child_display_name));
						}
						break;
					default:
						break;
					}
				}
			}
		}

		bool category_is_empty(const ic4::Property& prop)
		{
			int cnt = 0;
			for (auto&& feature : prop.asCategory().getFeatures())
			{
				switch (feature.getType())
				{
				case ic4::PropType::Integer:
				case ic4::PropType::Command:
				case ic4::PropType::String:
				case ic4::PropType::Enumeration:
				case ic4::PropType::Boolean:
				case ic4::PropType::Float:
				case ic4::PropType::Category:
					cnt++;
					break;
				default:
					break;
				}
			}
			return cnt > 0;
		}

		int num_children() const
		{
			const_cast<PropertyTreeNode*>(this)->populate();

			return static_cast<int>(children.size());
		}

		PropertyTreeNode* child(int n) const
		{
			const_cast<PropertyTreeNode*>(this)->populate();

			if (n < children.size())
				return children[n].get();

			return nullptr;
		}

		void register_notification_once(std::function<void(PropertyTreeNode*)> item_changed)
		{
			if (notification_token)
				return;

			prev_available = prop.isAvailable();

			notification_token = prop.eventAddNotification(
				[this, item_changed](ic4::Property& prop)
				{
					bool new_available = prop.isAvailable();
					if (prev_available != new_available)
					{
						item_changed(this);
						prev_available = new_available;
					}
				}
			);
		}
	};

	class PropertyTreeModel : public QAbstractItemModel
	{
	private:
		PropertyTreeNode tree_root_;
		PropertyTreeNode* prop_root_ = nullptr;

	public:
		PropertyTreeModel(ic4::PropCategory root)
			: QAbstractItemModel(Q_NULLPTR)
			, tree_root_(nullptr, root, 0, "", "")
		{
			// Add extra layer to make sure root element has a parent
			// QSortFilterProxyModel::filterAcceptsRow works with parent index
			tree_root_.children.push_back(std::make_unique<PropertyTreeNode>(&tree_root_, root, 0, QString::fromStdString(root.getName()), QString::fromStdString(root.getDisplayName())));
			prop_root_ = tree_root_.children.front().get();
		}

		~PropertyTreeModel()
		{
		}

		QModelIndex rootIndex() const
		{
			return createIndex(0, 0, prop_root_);
		}

	private:
		const PropertyTreeNode* parent_item(const QModelIndex& parent) const
		{
			if (!parent.isValid())
				return &tree_root_;

			return static_cast<const PropertyTreeNode*>(parent.internalPointer());
		}

	protected:
		QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override
		{
			if (!hasIndex(row, column, parent))
				return QModelIndex();

			auto* parentItem = parent_item(parent);

			auto* childItem = parentItem->child(row);
			if (childItem)
			{
				childItem->register_notification_once(
					[this](PropertyTreeNode* item)
					{
						auto item_index = createIndex(item->row, 0, item);

						const_cast<PropertyTreeModel*>(this)->dataChanged(item_index, item_index);
					}
				);

				return createIndex(row, column, childItem);
			}

			return QModelIndex();
		}
		QModelIndex parent(const QModelIndex& index) const override
		{
			if (!index.isValid())
				return QModelIndex();

			auto* childItem = static_cast<PropertyTreeNode*>(index.internalPointer());
			auto* parentItem = childItem->parent;

			if (parentItem == &tree_root_)
				return QModelIndex();

			return createIndex(parentItem->row, 0, parentItem);
		}
		int rowCount(const QModelIndex& parent = QModelIndex()) const override
		{
			if (parent.column() > 0)
				return 0;

			auto* parentItem = parent_item(parent);

			return parentItem->num_children();
		}
		int columnCount(const QModelIndex& parent = QModelIndex()) const override
		{
			Q_UNUSED(parent);
			return 2;
		}
		QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
		{
			if (!index.isValid())
				return QVariant();

			auto* tree = static_cast<PropertyTreeNode*>(index.internalPointer());
			switch (role)
			{
			case Qt::TextAlignmentRole:
				if (tree->children.size() == 0)
					return  static_cast<Qt::Alignment::Int>(Qt::AlignVCenter | Qt::AlignRight);
				else
					return  static_cast<Qt::Alignment::Int>(Qt::AlignVCenter | Qt::AlignLeft);
				break;
			case Qt::DisplayRole:
				if (index.column() == 0)
					return tree->display_name;
				return {};
			case Qt::ToolTipRole:
			{
				auto tt = tree->prop.getTooltip();
				auto desc = tree->prop.getDescription();

				if (!tt.empty()) return QString::fromStdString(tt);
				if (!desc.empty()) return QString::fromStdString(desc);
				return tree->display_name;
			}
			default:
				return {};
			}
		}
	};

	class PropertyTreeItemDelegate : public QStyledItemDelegate
	{
		QSortFilterProxyModel& proxy_;
		ic4::Grabber* grabber_;

	public:
		PropertyTreeItemDelegate(QSortFilterProxyModel& proxy, ic4::Grabber* grabber)
			: proxy_(proxy)
			, grabber_(grabber)
		{
		}

		QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& /* option */, const QModelIndex& index) const override
		{
			auto source_index = proxy_.mapToSource(index);

			auto* tree = static_cast<PropertyTreeNode*>(source_index.internalPointer());

			if (!tree)
			{
				return nullptr;
			}
				
			auto* widget = create_prop_control(tree->prop, parent, grabber_);
			if (widget)
			{
				widget->setContentsMargins(0, 0, 8, 0);
			}

			return widget;
		}
	};

	class TestItemDelegateForPaint : public QStyledItemDelegate
	{
		QSortFilterProxyModel& proxy_;
		QWidget* parent_ = nullptr;

	public:
		TestItemDelegateForPaint(QSortFilterProxyModel& proxy, QWidget* parent)
			: proxy_(proxy), parent_(parent)
		{
		}
	
		void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
		{
			
			auto source_index = proxy_.mapToSource(index);
			auto* tree = static_cast<PropertyTreeNode*>(source_index.internalPointer());
			
			if (tree->children.size() > 0) {
				// This paints the left half of the category title and background
				painter->save();
				painter->setPen(parent_->palette().color(QPalette::Text));
				auto r = option.rect;
				painter->fillRect(r, QBrush(parent_->palette().color(QPalette::Mid)));
				painter->drawText(r, option.displayAlignment, index.data().toString());
				painter->restore();
			}
			else
			{
				QStyledItemDelegate::paint(painter, option, index);
		
				/*
				painter->save();
				painter->setPen(parent_->palette().color(QPalette::Text));
				auto r = option.rect;
				painter->fillRect(r, QBrush(parent_->palette().color(QPalette::Background)));
				painter->drawText(r, option.displayAlignment, index.data().toString());
				painter->restore();
				*/				
			}
		}
	
	};


	class FilterPropertiesProxy : public QSortFilterProxyModel
	{
		QRegularExpression filter_regex_;
		ic4::PropVisibility visibility_ = ic4::PropVisibility::Expert;

		std::function<bool(const ic4::Property&)> filter_func_;

	public:
		FilterPropertiesProxy()
		{
			setRecursiveFilteringEnabled(true);
		}

		void filter(QString text, ic4::PropVisibility vis)
		{
			filter_regex_ = QRegularExpression(text, QRegularExpression::PatternOption::CaseInsensitiveOption);
			visibility_ = vis;
			invalidate();
		}

		void filter_func(std::function<bool(const ic4::Property&)> accept_prop)
		{
			filter_func_ = accept_prop;
			invalidate();
		}

	protected:

		// don't show blue selection
		Qt::ItemFlags flags(const QModelIndex& index) const 
		{
			const auto flags = QSortFilterProxyModel::flags(index);
			return flags & ~Qt::ItemIsSelectable;
		}

		bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
		{

			auto* tree = static_cast<PropertyTreeNode*>(sourceParent.internalPointer());
			if (!tree)
				return false;

			auto& child = *tree->children[sourceRow];

			if (child.display_name == "Software Transform")
			{
				qDebug() << (int)child.prop.getVisibility();
				int a = 10;
			}

			if (child.display_name == "Show Debug Overlay")
			{
				qDebug() << (int)child.prop.getVisibility();
				int a = 10;
			}

			if (child.display_name == "Action Control")
			{
				qDebug() << (int)child.prop.getVisibility();
				int a = 10;
			}

			if (!child.prop.isAvailable())
				return false;

			if (child.prop.getVisibility() > visibility_)
				return false;

			if (!filter_regex_.match(child.display_name).hasMatch() && !filter_regex_.match(child.prop_name).hasMatch())
				return false;

			if (!filter_func_)
				return true;

			return filter_func_(child.prop);
		}
	};


	class MyTreeView : public QTreeView
	{
		QSortFilterProxyModel& proxy_;
	public:
		MyTreeView(QSortFilterProxyModel& proxy)
			: proxy_(proxy)
		{

		}

	protected:

		void mousePressEvent(QMouseEvent* event) override
		{
			QModelIndex index = indexAt(event->pos());
			bool last_state = isExpanded(index);
			QTreeView::mousePressEvent(event);
			if (index.isValid() && last_state == isExpanded(index))
				setExpanded(index, !last_state);
		}

		void drawBranches(QPainter* painter,
			const QRect& rect,
			const QModelIndex& index) const
		{
			auto source_index = proxy_.mapToSource(index);
			auto* tree = static_cast<PropertyTreeNode*>(source_index.internalPointer());

			if (tree->children.size() > 0)
			{
				painter->fillRect(rect, QWidget::palette().color(QPalette::Mid));
				//painter->fillRect(rect, QColor(0xFFFF0000));

				// 
				int offset = (rect.width() - indentation()) / 2;
				auto c = QWidget::palette().color(QPalette::Text);
				int x = rect.x() + rect.width() / 2 + offset;
				int y = rect.y() + rect.height() / 2;
				int length = 9;

				if (isExpanded(index))
				{
					x = x - 5;
					y = y - 2;
					for (int i = 0; i < 5; ++i)
					{
						QRect arrow(x + i, y + i, length - (i * 2), 1);
						painter->fillRect(arrow, c);
					}
				}
				else
				{
					x = x - 2;
					y = y - 5;
					for (int i = 0; i < 5; ++i)
					{
						QRect arrow(x + i, y + i, 1, length - (i * 2));
						painter->fillRect(arrow, c);
					}
				}
			}
			else
			{
				QTreeView::drawBranches(painter, rect, index);
			}
		}

	};

	class PropertyTreeWidget : public QWidget
	{
	public:
		struct Settings
		{
			bool showRootItem = false;
			bool showInfoBox = true;
			bool showFilter = true;

			QString initialFilter;
			ic4::PropVisibility initialVisibility;
		};

	private:
		QComboBox* visibility_combo_ = nullptr;
		QLineEdit* filter_text_ = nullptr;

		PropertyInfoBox* info_text_ = nullptr;

		MyTreeView* view_ = nullptr;
		PropertyTreeModel* source_ = nullptr;
		FilterPropertiesProxy proxy_;

		PropertyTreeItemDelegate delegate_;
		TestItemDelegateForPaint branchPaintDelegate_;

		Settings settings_ = {};

	private:
		void update_visibility()
		{
			auto vis = static_cast<ic4::PropVisibility>(visibility_combo_->currentData().toInt());
			proxy_.filter(filter_text_->text(), vis);
		}

		void create_all_editors(QAbstractItemModel& model, QModelIndex parent)
		{
			int rows = model.rowCount(parent);

			for (int row = 0; row < rows; ++row)
			{
				auto index1 = model.index(row, 1, parent);
				view_->openPersistentEditor(index1);

				auto index0 = model.index(row, 0, parent);
				create_all_editors(model, index0);
			}
		}

		void propSelectionChanged(const QItemSelection& selected, const QItemSelection& /* deselected */)
		{
			if (!selected.isEmpty() && !selected.front().isEmpty())
			{
				auto item = selected.front().indexes().front();
				propSelected(item);
			}
			else
			{
				propSelected({});
			}
		}

		void propSelected(const QModelIndex& index)
		{
			if (!info_text_)
				return;

			auto source_index = proxy_.mapToSource(index);

			auto* tree = static_cast<PropertyTreeNode*>(source_index.internalPointer());
			if (!tree)
			{
				info_text_->clear();
				return;
			}

			info_text_->update(tree->prop);
		}

		void update_view()
		{
			if (!settings_.showRootItem && source_)
				view_->setRootIndex(proxy_.mapFromSource(source_->rootIndex()));

			create_all_editors(proxy_, {});
			view_->expandAll();

			bool source_available = source_ != nullptr;

			if (info_text_)
			{
				info_text_->setEnabled(source_available);
			}
			if (visibility_combo_)
			{
				visibility_combo_->setEnabled(source_available);
			}
			if (filter_text_)
			{
				filter_text_->setEnabled(source_available);
			}
			view_->setEnabled(source_available);

			if (source_available)
			{
				view_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
				view_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
				view_->header()->setStretchLastSection(true);
				view_->setItemDelegateForColumn(0, &branchPaintDelegate_);
				view_->setItemDelegateForColumn(1, &delegate_);
				view_->resizeColumnToContents(0);
			}
		}

		void proxyDataChanged(const QModelIndex& /* topLeft */, const QModelIndex& /* bottomRight */, const QVector<int>& /* roles = QVector<int>() */)
		{
			update_view();
		}

		void proxyLayoutChanged(const QList<QPersistentModelIndex>& /*parents = QList<QPersistentModelIndex>() */, QAbstractItemModel::LayoutChangeHint /* hint = QAbstractItemModel::NoLayoutChangeHint */)
		{
			update_view();
		}

	private:
		void updateModel(PropertyTreeModel* model)
		{
			auto* old_model = source_;

			source_ = model;
			proxy_.setSourceModel(source_);

			update_view();

			delete old_model;
		}

	public:
		void clearModel()
		{
			updateModel(nullptr);
		}
		void updateModel(ic4::PropCategory cat)
		{
			updateModel(new PropertyTreeModel(cat));
		}		

		void setPropertyFilter(std::function<bool(const ic4::Property&)> accept_prop)
		{
			proxy_.filter_func(accept_prop);
		}

	public:
		PropertyTreeWidget(ic4::PropCategory cat, ic4::Grabber* grabber, Settings settings = {}, QWidget* parent = nullptr)
			: PropertyTreeWidget(new PropertyTreeModel(cat), grabber, settings, parent)
		{
		}

	private:
		PropertyTreeWidget(PropertyTreeModel* model, ic4::Grabber* grabber, Settings settings = {}, QWidget* parent = nullptr)
			: QWidget(parent)
			, delegate_(proxy_, grabber)
			, branchPaintDelegate_(proxy_, this)
			, source_(model)
			, settings_(settings)
		{
			auto frame = new QFrame(this);
			auto* layout = new QVBoxLayout(frame);

			if (settings.showFilter)
			{
				auto* top = new QHBoxLayout;

				visibility_combo_ = new QComboBox;
				visibility_combo_->addItem("Beginner", (int)ic4::PropVisibility::Beginner);
				visibility_combo_->addItem("Expert", (int)ic4::PropVisibility::Expert);
				visibility_combo_->addItem("Guru", (int)ic4::PropVisibility::Guru);
				visibility_combo_->setCurrentIndex((int)settings.initialVisibility);
				visibility_combo_->setMinimumWidth(200);
				visibility_combo_->setStyleSheet("QComboBox {"
					"font-size: 13px;"
					"}");

				connect(visibility_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), [=](int)
					{
						update_visibility();
					});

				top->addWidget(visibility_combo_);

				filter_text_ = new QLineEdit;
				filter_text_->setStyleSheet("QLineEdit {"
					"font-size: 13px;"
					"}");
				filter_text_->setText(settings.initialFilter);
				connect(filter_text_, &QLineEdit::textChanged, this, [this](const QString&) { update_visibility(); });
				top->addWidget(filter_text_);
	
				layout->addLayout(top);
				update_visibility();
			}

			view_ = new MyTreeView(proxy_);
			view_->setStyleSheet("QTreeView::branch, QTreeView::item, QTreeView { "
				"outline: none; "
				"show-decoration-selected: 0;"
				"color: palette(text);"
				"background: palette(window);" // needs to be set,
				// in order that dottet line is not visible?!
				"font-size: 13px;"
			"}"
			"QTreeView::branch:open:adjoins-item:has-children{"
				"background: transparent;" // needs to be transparent, 
				//in order that QTReeView::drawBranches works?!
				"margin : 0;"
				" }"
			"QTreeView::branch:closed:adjoins-item:has-children{"
					"background: transparent;"// needs to be transparent, 
				//in order that QTReeView::drawBranches works?!
					"margin : 0;"
				" }"
			);
			proxy_.setSourceModel(source_);
			proxy_.filter(settings.initialFilter, settings.initialVisibility);

			view_->setModel(&proxy_);
			view_->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
			view_->header()->setHidden(true);
			view_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
			view_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
			view_->header()->setStretchLastSection(true);
			view_->setItemDelegateForColumn(0, &branchPaintDelegate_);
			view_->setItemDelegateForColumn(1, &delegate_);

			connect(view_, &QTreeView::clicked, this, &PropertyTreeWidget::propSelected);
			connect(view_->selectionModel(), &QItemSelectionModel::selectionChanged, this, &PropertyTreeWidget::propSelectionChanged);

			connect(&proxy_, &QSortFilterProxyModel::dataChanged, this, &PropertyTreeWidget::proxyDataChanged);
			connect(&proxy_, &QSortFilterProxyModel::layoutChanged, this, &PropertyTreeWidget::proxyLayoutChanged);

			if (settings_.showInfoBox)
			{
				info_text_ = new PropertyInfoBox(this);

				auto* splitter = new QSplitter(Qt::Orientation::Vertical, this);
				layout->addWidget(splitter);

				splitter->addWidget(view_);
				splitter->addWidget(info_text_);
				splitter->setStretchFactor(0, 3);
			}
			else
			{
				layout->addWidget(view_);
			}

			layout->setSpacing(0);
			layout->setContentsMargins(0, 0, 0, 0);

			frame->setLayout(layout);

			setLayout(layout);
			update_view();
		}

		~PropertyTreeWidget()
		{
			delete source_;
		}
	};
}