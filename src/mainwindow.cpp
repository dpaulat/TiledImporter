#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QDropEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QSet>

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent), ui(new Ui::MainWindow), image_(nullptr)
{
   ui->setupUi(this);
   ui->imageLabel->installEventFilter(this);
   ui->imageLabel->setAcceptDrops(true);

   statusLabel_ = new QLabel();
   ui->statusbar->addWidget(statusLabel_);
}

MainWindow::~MainWindow()
{
   delete ui;
   delete image_;
   delete statusLabel_;
}

bool MainWindow::eventFilter(QObject* object, QEvent* event)
{
   if (object == ui->imageLabel)
   {
      if (event->type() == QEvent::DragEnter)
      {
         QDragEnterEvent* dee = static_cast<QDragEnterEvent*>(event);
         dee->acceptProposedAction();
         return true;
      }
      else if (event->type() == QEvent::Drop)
      {
         QDropEvent* de = static_cast<QDropEvent*>(event);

         if (de->mimeData()->hasUrls())
         {
            QString path = de->mimeData()->urls().first().toLocalFile();
            qDebug("Dropped file onto canvas: " + path.toUtf8());
            OpenImage(path);
            return true;
         }
      }
   }

   return false;
}

void MainWindow::on_exitButton_clicked()
{
   QApplication::quit();
}

void MainWindow::on_importImageButton_clicked()
{
   QString path = QFileDialog::getOpenFileName(
      this, tr("Import Image..."), QString(), tr("Image Files (*.png *.bmp)"));

   if (!QFile::exists(path))
   {
      return;
   }

   qDebug("Opened image: " + path.toUtf8());
   OpenImage(path);
}

void MainWindow::OpenImage(const QString& path)
{
   QImage*    newImage = new QImage(path);
   QImage*    oldImage = image_;
   QImage     image(path);
   QSet<QRgb> colors;

   if (newImage->isNull())
   {
      QMessageBox::warning(
         this, tr("Import Image"), tr("Invalid image:\n") + path);
      qWarning("Invalid image: " + path.toUtf8());
      delete newImage;
      return;
   }

   QSize size(newImage->size());
   for (int y = 0; y < size.height(); y++)
   {
      for (int x = 0; x < size.width(); x++)
      {
         colors.insert(newImage->pixel(x, y));
      }
   }

   if (colors.size() > 2)
   {
      QMessageBox::warning(this,
                           tr("Import Image"),
                           tr("Image contains more than two colors:\n") + path);
      qWarning("Image contains more than two colors: " + path.toUtf8());
      delete newImage;
      return;
   }

   ui->imageLabel->setPixmap(QPixmap::fromImage(*newImage));
   image_ = newImage;
   delete oldImage;

   ui->saveImageButton->setEnabled(true);
   ui->transformGroupBox->setEnabled(true);

   statusLabel_->setText("Image Size: " + QString::number(size.width()) + "x" +
                         QString::number(size.height()));
}
