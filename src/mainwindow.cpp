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

static double luminance(const QColor& color);

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
    QMainWindow(parent), ui(new Ui::MainWindow), image_(nullptr)
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
         FitToScreen(ui->fitScreenButton->isChecked());
         return true;
      }
   }

   return false;
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
   Q_UNUSED(event)

   FitToScreen(ui->fitScreenButton->isChecked());
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
   FitToScreen(checked);
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

void MainWindow::FitToScreen(bool fitEnabled)
{
   if (fitEnabled)
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

      if (luminance(deadColor) > luminance(aliveColor))
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

void MainWindow::SetAliveDeadColors(QColor aliveColor, QColor deadColor)
{
   aliveColor_ = aliveColor;
   deadColor_  = deadColor;

   ui->aliveColorBox->setStyleSheet(
      "QLabel { background-color: " + aliveColor_.name() + "; }");
   ui->deadColorBox->setStyleSheet(
      "QLabel { background-color: " + deadColor_.name() + "; }");
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

static double luminance(const QColor& color)
{
   return 0.2126 * color.red() + 0.7152 * color.green() + 0.0722 * color.blue();
}
