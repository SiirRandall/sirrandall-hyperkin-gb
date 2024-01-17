#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QStyle>
#include <QStyleOption>
#include <QMimeData>
#include <QPainter>
#include <QAction>
#include <QFileInfo>
#include <QFileDialog>
#include <QMenu>

#include "filedropwidget.h"
#include "playlistentrydialog.h"
#include "../ui_qt.h"

extern "C" {
#include "../../../file_path_special.h"
}

FileDropWidget::FileDropWidget(QWidget *parent) :
   QWidget(parent)
{
   setAcceptDrops(true);
}

void FileDropWidget::paintEvent(QPaintEvent *event)
{
   QStyleOption o;
   QPainter p;
   o.initFrom(this);
   p.begin(this);
   style()->drawPrimitive(
      QStyle::PE_Widget, &o, &p, this);
   p.end();

   QWidget::paintEvent(event);
}

void FileDropWidget::keyPressEvent(QKeyEvent *event)
{
   if (event->key() == Qt::Key_Delete)
   {
      event->accept();
      emit deletePressed();
   }
   else
      QWidget::keyPressEvent(event);
}

void FileDropWidget::dragEnterEvent(QDragEnterEvent *event)
{
   const QMimeData *data = event->mimeData();

   if (data->hasUrls())
      event->acceptProposedAction();
}

void FileDropWidget::dropEvent(QDropEvent *event)
{
   const QMimeData *data = event->mimeData();

   if (data->hasUrls())
   {
      QList<QUrl> urls = data->urls();
      QStringList files;
      int i;

      for (i = 0; i < urls.count(); i++)
      {
         QString path(urls.at(i).toLocalFile());

         files.append(path);
      }

      emit filesDropped(files);
   }
}

void MainWindow::onFileDropWidgetContextMenuRequested(const QPoint &pos)
{
   QScopedPointer<QMenu> menu;
   QScopedPointer<QAction> downloadThumbnailAction;
   QScopedPointer<QAction> addEntryAction;
   QScopedPointer<QAction> addFilesAction;
   QScopedPointer<QAction> addFolderAction;
   QScopedPointer<QAction> editAction;
   QScopedPointer<QAction> deleteAction;
   QPointer<QAction> selectedAction;
   QPoint cursorPos = QCursor::pos();
   QHash<QString, QString> contentHash = getCurrentContentHash();

   if (m_browserAndPlaylistTabWidget->tabText(m_browserAndPlaylistTabWidget->currentIndex()) != msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS))
      return;

   menu.reset(new QMenu(this));

   downloadThumbnailAction.reset(new QAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_THUMBNAIL)), this));
   addEntryAction.reset(new QAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ADD_ENTRY)), this));
   addFilesAction.reset(new QAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ADD_FILES)), this));
   addFolderAction.reset(new QAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ADD_FOLDER)), this));
   editAction.reset(new QAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_EDIT)), this));
   deleteAction.reset(new QAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_DELETE)), this));

   menu->addAction(downloadThumbnailAction.data());
   menu->addAction(addEntryAction.data());
   menu->addAction(addFilesAction.data());
   menu->addAction(addFolderAction.data());

   if (!contentHash.isEmpty())
   {
      menu->addAction(editAction.data());
      menu->addAction(deleteAction.data());
   }

   selectedAction = menu->exec(cursorPos);

   if (!selectedAction)
      return;

   if (selectedAction == downloadThumbnailAction.data())
   {
      QHash<QString, QString> hash = getCurrentContentHash();
      QString system = QFileInfo(getCurrentPlaylistPath()).completeBaseName();
      QString title = hash.value("label");

      if (!title.isEmpty())
      {
         if (m_pendingThumbnailDownloadTypes.isEmpty())
         {
            m_pendingThumbnailDownloadTypes.append(THUMBNAIL_BOXART);
            m_pendingThumbnailDownloadTypes.append(THUMBNAIL_SCREENSHOT);
            m_pendingThumbnailDownloadTypes.append(THUMBNAIL_TITLE);
            downloadThumbnail(system, title);
         }
         else
         {
            showMessageBox(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALREADY_IN_PROGRESS), MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal, false);
         }
      }
   }
   else if (selectedAction == addFilesAction.data())
   {
      QStringList filePaths = QFileDialog::getOpenFileNames(this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SELECT_FILES));

      if (!filePaths.isEmpty())
         addFilesToPlaylist(filePaths);
   }
   else if (selectedAction == addEntryAction.data())
   {
      addFilesToPlaylist(QStringList());
   }
   else if (selectedAction == addFolderAction.data())
   {
      QString dirPath = QFileDialog::getExistingDirectory(this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SELECT_FOLDER), QString(), QFileDialog::ShowDirsOnly);

      if (!dirPath.isEmpty())
         addFilesToPlaylist(QStringList() << dirPath);
   }
   else if (selectedAction == editAction.data())
   {
      PlaylistEntryDialog *playlistDialog = playlistEntryDialog();
      QHash<QString, QString> selectedCore;
      QString selectedDatabase;
      QString selectedName;
      QString selectedPath;
      QString currentPlaylistPath = getCurrentPlaylistPath();

      if (!playlistDialog->showDialog(contentHash))
         return;

      selectedName = m_playlistEntryDialog->getSelectedName();
      selectedPath = m_playlistEntryDialog->getSelectedPath();
      selectedCore = m_playlistEntryDialog->getSelectedCore();
      selectedDatabase = m_playlistEntryDialog->getSelectedDatabase();

      if (selectedCore.isEmpty())
      {
         selectedCore["core_name"] = "DETECT";
         selectedCore["core_path"] = "DETECT";
      }

      if (selectedDatabase.isEmpty())
      {
         selectedDatabase = QFileInfo(currentPlaylistPath).fileName().remove(file_path_str(FILE_PATH_LPL_EXTENSION));
      }

      contentHash["label"] = selectedName;
      contentHash["path"] = selectedPath;
      contentHash["core_name"] = selectedCore.value("core_name");
      contentHash["core_path"] = selectedCore.value("core_path");
      contentHash["db_name"] = selectedDatabase;

      if (!updateCurrentPlaylistEntry(contentHash))
      {
         showMessageBox(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_UPDATE_PLAYLIST_ENTRY), MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal, false);
         return;
      }
   }
   else if (selectedAction == deleteAction.data())
   {
      deleteCurrentPlaylistItem();
   }
}
