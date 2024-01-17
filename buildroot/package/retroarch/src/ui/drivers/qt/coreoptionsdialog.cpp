#include <QCloseEvent>
#include <QResizeEvent>
#include <QMessageBox>
#include <QPainter>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QMenu>
#include <QFileDialog>
#include <QTimer>

#include "coreoptionsdialog.h"
#include "../ui_qt.h"

extern "C" {
#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <file/file_path.h>
#include "../../../command.h"
#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../paths.h"
#include "../../../file_path_special.h"
#include "../../../managers/core_option_manager.h"
}

CoreOptionsDialog::CoreOptionsDialog(QWidget *parent) :
   QDialog(parent)
   ,m_layout()
   ,m_scrollArea()
{
   setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS));
   setObjectName("coreOptionsDialog");

   resize(720, 480);

   QTimer::singleShot(0, this, SLOT(clearLayout()));
}

CoreOptionsDialog::~CoreOptionsDialog()
{
}

void CoreOptionsDialog::resizeEvent(QResizeEvent *event)
{
   QDialog::resizeEvent(event);

   if (!m_scrollArea)
      return;

   m_scrollArea->resize(event->size());

   emit resized(event->size());
}

void CoreOptionsDialog::closeEvent(QCloseEvent *event)
{
   QDialog::closeEvent(event);

   emit closed();
}

void CoreOptionsDialog::paintEvent(QPaintEvent *event)
{
   QStyleOption o;
   QPainter p;
   o.initFrom(this);
   p.begin(this);
   style()->drawPrimitive(
      QStyle::PE_Widget, &o, &p, this);
   p.end();

   QDialog::paintEvent(event);
}

void CoreOptionsDialog::clearLayout()
{
   QWidget *widget = NULL;

   if (m_scrollArea)
   {
      foreach (QObject *obj, children())
      {
         obj->deleteLater();
      }
   }

   m_layout = new QVBoxLayout();

   widget = new QWidget();
   widget->setLayout(m_layout);
   widget->setObjectName("coreOptionsWidget");

   m_scrollArea = new QScrollArea();

   m_scrollArea->setParent(this);
   m_scrollArea->setWidgetResizable(true);
   m_scrollArea->setWidget(widget);
   m_scrollArea->setObjectName("coreOptionsScrollArea");
   m_scrollArea->show();
}

void CoreOptionsDialog::reload()
{
   buildLayout();
}

void CoreOptionsDialog::onSaveGameSpecificOptions()
{
   char game_path[PATH_MAX_LENGTH];
   config_file_t *conf = NULL;

   game_path[0] = '\0';

   if (!retroarch_validate_game_options(game_path, sizeof(game_path), true))
   {
      QMessageBox::critical(this, msg_hash_to_str(MSG_ERROR), msg_hash_to_str(MSG_ERROR_SAVING_CORE_OPTIONS_FILE));
      return;
   }

   conf = config_file_new(game_path);

   if (!conf)
   {
      conf = config_file_new(NULL);

      if (!conf)
      {
         QMessageBox::critical(this, msg_hash_to_str(MSG_ERROR), msg_hash_to_str(MSG_ERROR_SAVING_CORE_OPTIONS_FILE));
         return;
      }
   }

   if (config_file_write(conf, game_path))
   {
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY),
            1, 100, true);
      path_set(RARCH_PATH_CORE_OPTIONS, game_path);
   }

   config_file_free(conf);
}

void CoreOptionsDialog::onCoreOptionComboBoxCurrentIndexChanged(int index)
{
   QComboBox *comboBox = qobject_cast<QComboBox*>(sender());
   QString key, val;
   size_t opts = 0;
   unsigned i, k;

   if (!comboBox)
      return;

   key = comboBox->itemData(index, Qt::UserRole).toString();
   val = comboBox->itemText(index);

   if (rarch_ctl(RARCH_CTL_HAS_CORE_OPTIONS, NULL))
   {
      rarch_ctl(RARCH_CTL_GET_CORE_OPTION_SIZE, &opts);

      if (opts)
      {
         core_option_manager_t *coreopts = NULL;

         rarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts);

         if (coreopts)
         {
            for (i = 0; i < opts; i++)
            {
               QString optKey;
               struct core_option *option = static_cast<struct core_option*>(&coreopts->opts[i]);

               if (!option)
                  continue;

               optKey = option->key;

               if (key == optKey)
               {
                  for (k = 0; k < option->vals->size; k++)
                  {
                     QString str = option->vals->elems[k].data;

                     if (!str.isEmpty() && str == val)
                        core_option_manager_set_val(coreopts, i, k);
                  }
               }
            }
         }
      }
   }
}

void CoreOptionsDialog::buildLayout()
{
   QFormLayout *form = NULL;
   settings_t *settings = config_get_ptr();
   size_t opts = 0;
   int i;
   unsigned j, k;

   clearLayout();

   if (rarch_ctl(RARCH_CTL_HAS_CORE_OPTIONS, NULL))
   {
      rarch_ctl(RARCH_CTL_GET_CORE_OPTION_SIZE, &opts);

      if (opts)
      {
         core_option_manager_t *coreopts = NULL;

         form = new QFormLayout();

         if (settings->bools.game_specific_options)
         {
            rarch_system_info_t *system = runloop_get_system_info();
            QString contentLabel;
            QString label;

            if (system)
               contentLabel = QFileInfo(path_get(RARCH_PATH_BASENAME)).completeBaseName();

            if (!contentLabel.isEmpty())
            {
               if (!rarch_ctl(RARCH_CTL_IS_GAME_OPTIONS_ACTIVE, NULL))
                  label = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE);
               else
                  label = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE);

               if (!label.isEmpty())
               {
                  QHBoxLayout *gameOptionsLayout = new QHBoxLayout();
                  QPushButton *button = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SAVE), this);

                  connect(button, SIGNAL(clicked()), this, SLOT(onSaveGameSpecificOptions()));

                  gameOptionsLayout->addWidget(new QLabel(contentLabel, this));
                  gameOptionsLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Preferred));
                  gameOptionsLayout->addWidget(button);

                  form->addRow(label, gameOptionsLayout);
               }
            }
         }

         rarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts);

         if (coreopts)
         {
            for (j = 0; j < opts; j++)
            {
               QString desc = core_option_manager_get_desc(coreopts, j);
               QString val = core_option_manager_get_val(coreopts, j);
               QComboBox *comboBox = NULL;
               struct core_option *option = NULL;

               if (desc.isEmpty() || !coreopts->opts)
                  continue;

               option = static_cast<struct core_option*>(&coreopts->opts[j]);

               if (!option->vals || option->vals->size == 0)
                  continue;

               comboBox = new QComboBox(this);

               for (k = 0; k < option->vals->size; k++)
               {
                  comboBox->addItem(option->vals->elems[k].data, option->key);
               }

               comboBox->setCurrentText(val);

               /* Only connect the signal after setting the default item */
               connect(comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onCoreOptionComboBoxCurrentIndexChanged(int)));

               form->addRow(desc, comboBox);
            }

            m_layout->addLayout(form);
         }
      }
   }

   if (!opts)
   {
      QLabel *noParamsLabel = new QLabel(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE), this);
      noParamsLabel->setAlignment(Qt::AlignCenter);

      m_layout->addWidget(noParamsLabel);
   }

   m_layout->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));

   resize(width() + 1, height());
   show();
   resize(width() - 1, height());
}
