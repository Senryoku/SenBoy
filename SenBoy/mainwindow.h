#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <SFML/Graphics.hpp>
#include <QWidget>
#include <QTimer>

class QSFMLCanvas : public QWidget, public sf::RenderWindow
{
public :

    QSFMLCanvas(QWidget* Parent, const QPoint& Position, const QSize& Size, unsigned int FrameTime = 0);

    virtual ~QSFMLCanvas() =default;

private :

    virtual void OnInit() =0;

    virtual void OnUpdate() =0;

    virtual QPaintEngine* paintEngine() const;

    virtual void showEvent(QShowEvent*);

    virtual void paintEvent(QPaintEvent*);

    QTimer myTimer;
    bool   myInitialized;
};

class MainCanvas : public QSFMLCanvas
{
public:
    MainCanvas(QWidget* Parent, const QPoint& Position, const QSize& Size);
private:
    virtual void OnInit() override;
    virtual void OnUpdate() override;
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

private:
    Ui::MainWindow *ui;

    void open_rom(const QString& path);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
};

#endif // MAINWINDOW_H
