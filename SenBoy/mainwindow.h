#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "QSFMLCanvas.hpp"

class MainCanvas : public QSFMLCanvas
{
public:
    MainCanvas(QWidget* Parent, const QPoint& Position, const QSize& Size);
private:
    virtual void OnInit() override;
    virtual void OnUpdate() override;
};

#include "ui_debugwindow.h"

class DebugWindow : public QWidget
{
    Q_OBJECT
public:
    DebugWindow(QWidget* Parent = nullptr);
    virtual ~DebugWindow();

private:
    Ui::DebugWindow *ui;
};

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void action_open();
    void on_actionDebug_Info_triggered();

private:
    Ui::MainWindow *ui;

    DebugWindow*    _debug_window = nullptr;

    void open_rom(const QString& path);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
};

#endif // MAINWINDOW_H
