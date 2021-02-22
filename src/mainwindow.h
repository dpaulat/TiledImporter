#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <QColor>
#include <QLabel>
#include <QProgressBar>
#include <QPromise>

QT_BEGIN_NAMESPACE
namespace Ui
{
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
   Q_OBJECT

public:
   MainWindow(QWidget* parent = nullptr);
   ~MainWindow();

   bool eventFilter(QObject* object, QEvent* event) override;

private slots:
   void on_cellularAutomataOption_toggled(bool checked);
   void on_colorReverseButton_clicked();
   void on_exitButton_clicked();
   void on_importImageButton_clicked();
   void on_transformButton_clicked();

private:
   Ui::MainWindow* ui;

   QImage* image_;

   QProgressBar* progressBar_;

   QColor aliveColor_;
   QColor deadColor_;

   int  GetMooreNeighborsAlive(const QImage& image, int x, int y);
   void OpenImage(const QString& path);
   void SetAliveDeadColors(QColor aliveColor, QColor deadColor);
   void TransformImage(QPromise<void>& promise,
                       QImage*         image,
                       const QColor&   newAliveColor,
                       const QColor&   newDeadColor,
                       size_t          iterations);
};
#endif // MAINWINDOW_H
