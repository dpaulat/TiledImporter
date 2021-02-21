#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <QLabel>

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

   bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
   void on_exitButton_clicked();
   void on_importImageButton_clicked();

private:
   Ui::MainWindow* ui;

   QImage* image_;
   QLabel* statusLabel_;

   void OpenImage(const QString& path);
};
#endif // MAINWINDOW_H
