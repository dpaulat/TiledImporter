#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QDropEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QSet>
#include <QStyle>

static double luminance(const QColor& color);

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent), ui(new Ui::MainWindow), image_(nullptr)
{
   ui->setupUi(this);
   ui->bottomDock->setVisible(false);
   ui->cellularGroupBox->setVisible(false);

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

void MainWindow::on_cellularAutomataOption_toggled(bool checked)
{
   ui->cellularGroupBox->setVisible(checked);
}

void MainWindow::on_colorReverseButton_clicked()
{
   SetAliveDeadColors(deadColor_, aliveColor_);
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

void MainWindow::on_transformButton_clicked()
{
   // Do something
   QImage* newImage = new QImage(*image_);
   QImage* oldImage = image_;

   const QSize size(newImage->size());

   const int aliveNeighborsAllowed = ui->aliveNeighborsBox->value();
   const int deadNeighborsAllowed  = ui->deadNeighborsBox->value();
   const int iterations            = ui->iterationsSpinBox->value();

   QColor newAliveColor(aliveColor_);
   QColor newDeadColor(deadColor_);

   if (ui->invertCheckBox->isChecked())
   {
      newAliveColor = deadColor_;
      newDeadColor  = aliveColor_;
   }

   for (int i = 0; i < iterations; i++)
   {
      for (int y = 0; y < size.height(); y++)
      {
         for (int x = 0; x < size.width(); x++)
         {
            int aliveCells = GetMooreNeighborsAlive(*image_, x, y);
            if (image_->pixelColor(x, y) == aliveColor_)
            {
               if (aliveCells < aliveNeighborsAllowed)
               {
                  newImage->setPixelColor(x, y, newDeadColor);
               }
               else
               {
                  newImage->setPixelColor(x, y, newAliveColor);
               }
            }
            else
            {
               if (aliveCells > deadNeighborsAllowed)
               {
                  newImage->setPixelColor(x, y, newAliveColor);
               }
               else
               {
                  newImage->setPixelColor(x, y, newDeadColor);
               }
            }
         }
      }
   }

   ui->imageLabel->setPixmap(QPixmap::fromImage(*newImage));
   image_ = newImage;
   delete oldImage;
}

int MainWindow::GetMooreNeighborsAlive(const QImage& image, int x, int y)
{
   const QSize size(image.size());

   int aliveNearbyCells = 0;

   for (int i = -1; i < 2; i++)
   {
      for (int j = -1; j < 2; j++)
      {
         int nearbyX = x + i;
         int nearbyY = y + j;

         if (i == 0 && j == 0)
         {
            continue;
         }
         else if (nearbyX < 0 || nearbyY < 0 || nearbyX >= size.width() ||
                  nearbyY >= size.height() ||
                  image.pixelColor(nearbyX, nearbyY) == aliveColor_)
         {
            aliveNearbyCells++;
         }
      }
   }

   return aliveNearbyCells;
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

   const QSize size(newImage->size());
   for (int y = 0; y < size.height(); y++)
   {
      for (int x = 0; x < size.width(); x++)
      {
         colors.insert(newImage->pixel(x, y));
      }
   }
   qDebug() << "Image colors: " << colors.size();

   if (colors.size() > 2)
   {
      QIcon icon = style()->standardIcon(QStyle::SP_MessageBoxWarning);
      ui->cellularAutomataOption->setIcon(icon);
      ui->cellularAutomataOption->setToolTip(
         "Image contains more than two colors");
      ui->cellularAutomataOption->setChecked(false);
      ui->cellularAutomataOption->setCheckable(false);
   }
   else
   {
      ui->cellularAutomataOption->setIcon(QIcon());
      ui->cellularAutomataOption->setToolTip("");
      ui->cellularAutomataOption->setCheckable(true);

      QColor aliveColor;
      QColor deadColor;

      aliveColor = *(colors.begin());
      if (colors.size() == 2)
      {
         deadColor = *(++colors.begin());
      }
      else if (aliveColor != 0xFF000000u)
      {
         deadColor = 0xFF000000u;
      }
      else
      {
         deadColor = 0xFFFFFFFFu;
      }

      if (luminance(deadColor) > luminance(aliveColor))
      {
         QColor tempColor(deadColor);
         deadColor  = aliveColor;
         aliveColor = tempColor;
      }

      SetAliveDeadColors(aliveColor, deadColor);
   }

   ui->imageLabel->setPixmap(QPixmap::fromImage(*newImage));
   image_ = newImage;
   delete oldImage;

   ui->saveImageButton->setEnabled(true);
   ui->transformGroupBox->setEnabled(true);

   statusLabel_->setText("Image Size: " + QString::number(size.width()) + "x" +
                         QString::number(size.height()));
}

void MainWindow::SetAliveDeadColors(QColor aliveColor, QColor deadColor)
{
   aliveColor_ = aliveColor;
   deadColor_  = deadColor;

   ui->aliveColorBox->setStyleSheet(
      "QLabel { background-color: " + aliveColor_.name() + "; }");
   ui->deadColorBox->setStyleSheet(
      "QLabel { background-color: " + deadColor_.name() + "; }");
}

static double luminance(const QColor& color)
{
   return 0.2126 * color.red() + 0.7152 * color.green() + 0.0722 * color.blue();
}
