#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QDropEvent>
#include <QFileDialog>
#include <QFuture>
#include <QFutureWatcher>
#include <QGraphicsSceneDragDropEvent>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QSet>
#include <QStyle>
#include <QToolBar>

static double getScale(const QTransform& t);
static double getLuminance(const QColor& color);

class ImageGraphicsScene : public QGraphicsScene
{
private:
   MainWindow* mainWindow_;

public:
   ImageGraphicsScene(MainWindow* parent = nullptr) :
       QGraphicsScene(parent), mainWindow_(parent)
   {
   }

   void dragMoveEvent(QGraphicsSceneDragDropEvent* event)
   {
      event->acceptProposedAction();
   }

   void dropEvent(QGraphicsSceneDragDropEvent* event)
   {
      if (event->mimeData()->hasUrls())
      {
         QString path = event->mimeData()->urls().first().toLocalFile();
         qDebug("Dropped file onto scene: " + path.toUtf8());
         mainWindow_->OpenImage(path);
      }
   }
};

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    currentDir_(),
    image_(nullptr),
    resizeInProgress_(false)
{
   ui->setupUi(this);
   ui->bottomDock->setVisible(false);
   ui->cellularGroupBox->setVisible(false);
   ui->imageControlFrame->setVisible(false);

   ui->imageView->installEventFilter(this);
   ui->imageView->setAcceptDrops(true);

   imageScene_ = new ImageGraphicsScene(this);

   ui->imageView->setScene(imageScene_);

   progressBar_ = new QProgressBar(this);
   progressBar_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
   progressBar_->setMaximumHeight(ui->iterationsLabel->height());
   progressBar_->setTextVisible(false);
   progressBar_->setVisible(false);
   ui->statusbar->addPermanentWidget(progressBar_);

   QMenu* zoomMenu = new QMenu(this);
   zoomMenu->addAction("10%", this, SLOT(ImageZoomSelected()));
   zoomMenu->addAction("25%", this, SLOT(ImageZoomSelected()));
   zoomMenu->addAction("50%", this, SLOT(ImageZoomSelected()));
   zoomMenu->addAction("75%", this, SLOT(ImageZoomSelected()));
   zoomMenu->addAction("100%", this, SLOT(ImageZoomSelected()));
   zoomMenu->addAction("150%", this, SLOT(ImageZoomSelected()));
   zoomMenu->addAction("200%", this, SLOT(ImageZoomSelected()));
   zoomMenu->addAction("300%", this, SLOT(ImageZoomSelected()));
   zoomMenu->addAction("400%", this, SLOT(ImageZoomSelected()));
   zoomMenu->addAction("...", this, SLOT(ImageZoomSelected()));

   ui->zoomValueButton->setMenu(zoomMenu);
}

MainWindow::~MainWindow()
{
   delete ui;
   delete image_;
   delete imageScene_;
   delete progressBar_;
}

bool MainWindow::eventFilter(QObject* object, QEvent* event)
{
   if (object == ui->imageView)
   {
      if (event->type() == QEvent::DragEnter)
      {
         QDragEnterEvent* dee = static_cast<QDragEnterEvent*>(event);
         dee->acceptProposedAction();
         return true;
      }
      else if (event->type() == QEvent::DragMove)
      {
         QDragMoveEvent* dme = static_cast<QDragMoveEvent*>(event);
         dme->acceptProposedAction();
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
      else if (event->type() == QEvent::Resize)
      {
         ResizeImage();
         return true;
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

void MainWindow::on_fitScreenButton_toggled(bool checked)
{
   if (!resizeInProgress_)
   {
      resizeInProgress_ = true;

      if (checked)
      {
         ui->originalSizeButton->setChecked(false);
      }

      ResizeImage();

      resizeInProgress_ = false;
   }
}

void MainWindow::on_importImageButton_clicked()
{
   QString path = QFileDialog::getOpenFileName(
      this, tr("Import Image"), QString(), tr("Image Files (*.png *.bmp)"));

   if (!QFile::exists(path))
   {
      return;
   }

   qDebug("Opened image: " + path.toUtf8());
   OpenImage(path);
}

void MainWindow::on_originalSizeButton_toggled(bool checked)
{
   if (!resizeInProgress_)
   {
      resizeInProgress_ = true;

      if (checked)
      {
         ui->fitScreenButton->setChecked(false);
      }

      ResizeImage();

      resizeInProgress_ = false;
   }
}

void MainWindow::on_saveImageButton_clicked()
{
   QString path = QFileDialog::getSaveFileName(
      this, tr("Save Image"), currentDir_.absolutePath(), tr("PNG (*.png)"));

   if (!path.isNull())
   {
      QFileInfo fileInfo(path);
      currentDir_  = fileInfo.absoluteDir();
      currentFile_ = path;

      if (!image_->save(path))
      {
         if (!image_->save(path, "PNG"))
         {
            QMessageBox::warning(
               this, tr("Save Image"), tr("Error attempting to save: ") + path);
         }
      }
   }
}

void MainWindow::on_transformButton_clicked()
{
   QImage* newImage = new QImage(*image_);

   const QSize size(newImage->size());
   const int   iterations = ui->iterationsSpinBox->value();

   QColor newAliveColor(aliveColor_);
   QColor newDeadColor(deadColor_);

   if (ui->invertCheckBox->isChecked())
   {
      newAliveColor = deadColor_;
      newDeadColor  = aliveColor_;
   }

   QPromise<void>        promise;
   QFuture<void>         future        = promise.future();
   QFutureWatcher<void>* futureWatcher = new QFutureWatcher<void>();

   futureWatcher->setFuture(future);

   progressBar_->setRange(0, iterations * size.height());
   progressBar_->setValue(0);
   progressBar_->setVisible(true);

   ui->imageGroupBox->setEnabled(false);
   ui->cellularGroupBox->setEnabled(false);

   QThread* thread = QThread::create(
      [=](QPromise<void> promise) {
         promise.start();
         TransformImage(
            promise, newImage, newAliveColor, newDeadColor, iterations);
         promise.finish();
      },
      std::move(promise));

   connect(futureWatcher,
           &QFutureWatcher<void>::progressValueChanged,
           this,
           [=](int progressValue) {
              progressBar_->setValue(progressValue);
              progressBar_->update();
           });
   connect(futureWatcher, &QFutureWatcher<void>::finished, this, [=]() {
      qDebug() << "Transform complete";
      ui->statusbar->showMessage(tr("Image transform complete"));

      progressBar_->setVisible(false);

      UpdateImage(newImage);

      ui->imageGroupBox->setEnabled(true);
      ui->cellularGroupBox->setEnabled(true);

      delete thread;
      delete futureWatcher;
   });

   qDebug() << "Starting transform...";
   ui->statusbar->showMessage(tr("Transforming image..."));
   thread->start();
}

void MainWindow::on_zoomInButton_clicked()
{
   double scale = GetImageScale();
   int    step  = round(floor(round(scale * 100.0) / 10.0));
   int    zoom  = (step + 1) * 10;
   int    max   = ui->zoomSlider->maximum();

   zoom = (zoom > max) ? max : zoom;

   SetImageZoom(zoom);
}

void MainWindow::on_zoomOutButton_clicked()
{
   double scale = GetImageScale();
   int    step  = round(ceil(round(scale * 100.0) / 10.0));
   int    zoom  = (step - 1) * 10;
   int    min   = ui->zoomSlider->minimum();

   zoom = (zoom < min) ? min : zoom;

   SetImageZoom(zoom);
}

void MainWindow::on_zoomSlider_sliderMoved(int position)
{
   SetImageZoom(position);
}

void MainWindow::ImageZoomSelected()
{
   QAction* action = qobject_cast<QAction*>(sender());

   if (action != nullptr)
   {
      int zoom = action->text().remove('%').toInt();
      if (zoom != 0)
      {
         SetImageZoom(zoom);
      }
   }
}

double MainWindow::GetImageScale()
{
   return getScale(ui->imageView->transform());
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

   QFileInfo fileInfo(path);
   currentDir_  = fileInfo.absoluteDir();
   currentFile_ = path;

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
      ui->cellularAutomataOption->setStatusTip(
         tr("Image contains more than two colors"));
      ui->cellularAutomataOption->setToolTip(
         tr("Image contains more than two colors"));
      ui->cellularAutomataOption->setChecked(false);
      ui->cellularAutomataOption->setCheckable(false);
   }
   else
   {
      ui->cellularAutomataOption->setIcon(QIcon());
      ui->cellularAutomataOption->setStatusTip("");
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

      if (getLuminance(deadColor) > getLuminance(aliveColor))
      {
         QColor tempColor(deadColor);
         deadColor  = aliveColor;
         aliveColor = tempColor;
      }

      SetAliveDeadColors(aliveColor, deadColor);
   }

   ui->imageSizeLabel->setText(tr("Image size: ") +
                               QString::number(size.width()) + "x" +
                               QString::number(size.height()));

   UpdateImage(newImage);

   ui->imageControlFrame->setVisible(true);

   ui->saveImageButton->setEnabled(true);
   ui->transformGroupBox->setEnabled(true);

   ui->statusbar->showMessage(tr("Loaded image with size ") +
                              QString::number(size.width()) + "x" +
                              QString::number(size.height()));
}

void MainWindow::ResizeImage()
{
   qDebug() << "Resizing image";

   if (ui->fitScreenButton->isChecked())
   {
      ui->imageView->update();
      ui->imageView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
      ui->imageView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
      ui->imageView->fitInView(imageScene_->sceneRect(), Qt::KeepAspectRatio);
   }
   {
      ui->imageView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
      ui->imageView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
   }

   if (ui->originalSizeButton->isChecked())
   {
      ui->imageView->resetTransform();
   }

   double scale        = GetImageScale();
   int    roundedScale = round(scale * 100);
   ui->zoomValueButton->setText(QString::number(roundedScale) + "%");
   ui->zoomSlider->setValue(roundedScale);

   qDebug() << "Scale: " << scale;
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

void MainWindow::SetImageZoom(int zoom)
{
   if (!resizeInProgress_)
   {
      resizeInProgress_ = true;

      qDebug() << "Set image zoom: " << zoom;

      ui->fitScreenButton->setChecked(false);
      ui->originalSizeButton->setChecked(false);

      double scale = zoom / 100.0;
      ui->imageView->resetTransform();
      ui->imageView->scale(scale, scale);

      ui->zoomValueButton->setText(QString::number(zoom) + "%");
      ui->zoomSlider->setValue(zoom);

      resizeInProgress_ = false;
   }
}

void MainWindow::TransformImage(QPromise<void>& promise,
                                QImage*         image,
                                const QColor&   newAliveColor,
                                const QColor&   newDeadColor,
                                size_t          iterations)
{
   const QSize size(image->size());
   const int   aliveNeighborsAllowed = ui->aliveNeighborsBox->value();
   const int   deadNeighborsAllowed  = ui->deadNeighborsBox->value();

   int progress = 0;

   for (size_t i = 0; i < iterations; i++)
   {
      QImage* lastImage = new QImage(*image);

      for (int y = 0; y < size.height(); y++)
      {
         // Update progress at the start of each row
         promise.setProgressValue(++progress);

         for (int x = 0; x < size.width(); x++)
         {
            int aliveCells = GetMooreNeighborsAlive(*lastImage, x, y);
            if (lastImage->pixelColor(x, y) == aliveColor_)
            {
               if (aliveCells < aliveNeighborsAllowed)
               {
                  image->setPixelColor(x, y, newDeadColor);
               }
               else
               {
                  image->setPixelColor(x, y, newAliveColor);
               }
            }
            else
            {
               if (aliveCells > deadNeighborsAllowed)
               {
                  image->setPixelColor(x, y, newAliveColor);
               }
               else
               {
                  image->setPixelColor(x, y, newDeadColor);
               }
            }
         }
      }

      delete lastImage;
   }
}

void MainWindow::UpdateImage(QImage* newImage)
{
   ui->imageView->scene()->clear();
   ui->imageView->scene()->addPixmap(QPixmap::fromImage(*newImage));

   QImage* oldImage = image_;
   image_           = newImage;
   delete oldImage;
}

static double getLuminance(const QColor& color)
{
   return 0.2126 * color.red() + 0.7152 * color.green() + 0.0722 * color.blue();
}

static double getScale(const QTransform& t)
{
   // No rotation:
   // X Scale: t.m11();
   // Y Scale: t.m12();

   // Rotation:
   // X Scale: sqrt(t.m11() * t.m11() + t.m12() * t.m12())
   // Y Scale: sqrt(t.m21() * t.m21() + t.m22() * t.m22())

   // Assume no rotation, x and y scales are equal
   return t.m11();
}
