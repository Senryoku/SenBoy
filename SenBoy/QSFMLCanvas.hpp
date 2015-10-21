#pragma once

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
