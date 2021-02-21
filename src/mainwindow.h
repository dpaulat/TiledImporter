#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <QLabel>
#include <QRgb>

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
   QLabel* statusLabel_;

   QColor aliveColor_;
   QColor deadColor_;

   int  GetMooreNeighborsAlive(const QImage& image, int x, int y);
   void OpenImage(const QString& path);
   void SetAliveDeadColors(QColor aliveColor, QColor deadColor);
};
#endif // MAINWINDOW_H
