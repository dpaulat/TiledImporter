#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <QColor>
#include <QDir>
#include <QGraphicsScene>
#include <QLabel>
#include <QProgressBar>
#include <QPromise>
#include <QSlider>
#include <QToolButton>

#include <deque>

QT_BEGIN_NAMESPACE
namespace Ui
{
class MainWindow;
}
QT_END_NAMESPACE

typedef std::deque<std::shared_ptr<QImage>> QImagePtrDeque;

class MainWindow : public QMainWindow
{
   Q_OBJECT

public:
   MainWindow(QWidget* parent = nullptr);
   ~MainWindow();

public:
   bool eventFilter(QObject* object, QEvent* event) override;

   void OpenImage(const QString& path);

private slots:
   void on_cellularAutomataOption_toggled(bool checked);
   void on_colorReverseButton_clicked();
   void on_exitButton_clicked();
   void on_fitScreenButton_toggled(bool checked);
   void on_importImageButton_clicked();
   void on_originalSizeButton_toggled(bool checked);
   void on_saveImageButton_clicked();
   void on_transformButton_clicked();
   void on_redoButton_clicked();
   void on_undoButton_clicked();
   void on_zoomInButton_clicked();
   void on_zoomOutButton_clicked();
   void on_zoomSlider_sliderMoved(int position);

   void ImageZoomSelected();

private:
   Ui::MainWindow* ui;

   QDir    currentDir_;
   QString currentFile_;

   QGraphicsScene* imageScene_;

   QImagePtrDeque           imageStack_;
   QImagePtrDeque::iterator currentImage_;

   QProgressBar* progressBar_;

   QColor aliveColor_;
   QColor deadColor_;

   bool resizeInProgress_;

   std::shared_ptr<QImage> CurrentImage();

   double GetImageScale();
   int    GetMooreNeighborsAlive(const QImage& image, int x, int y);
   void   ResizeImage();
   void   SetAliveDeadColors(QColor aliveColor, QColor deadColor);
   void   SetImageZoom(int zoom);
   void   TransformImage(QPromise<void>&         promise,
                         std::shared_ptr<QImage> image,
                         const QColor&           newAliveColor,
                         const QColor&           newDeadColor,
                         size_t                  iterations);
   void   UpdateImage(std::shared_ptr<QImage> newImage);
};
#endif // MAINWINDOW_H
