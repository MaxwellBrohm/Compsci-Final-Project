// These are all the necessary Qt libraries we need to use graphics, keyboard input, timers, and more.
#include <QApplication>             // Runs the Qt application
#include <QGraphicsScene>           // The "world" where all game objects live
#include <QGraphicsView>            // The window/frame that shows part of the scene
#include <QGraphicsRectItem>        // A rectangular game object (like our player or platforms)
#include <QGraphicsEllipseItem>     // A circular object (like our win circle)
#include <QGraphicsPolygonItem>     // Used for drawing triangle spikes
#include <QKeyEvent>                // Handles key presses
#include <QTimer>                   // Lets us run code repeatedly, like a game loop
#include <QSet>                     // Stores keys being pressed
#include <QDebug>                   // Useful for printing debug messages (not used here)
#include <QGraphicsTextItem>        // Used to draw text (like lives and level count)
#include <QRandomGenerator>         // Lets us create random numbers (used for procedural level generation)
#include <QGraphicsSimpleTextItem>  // Not used but useful for simple text rendering

//-----------------------------------------
// This class represents the player character.
// It's just a blue square that the user controls.
class Player : public QGraphicsRectItem {
public:
    Player() {
        setRect(0, 0, 20, 20);   // Set width and height to 20x20 pixels
        setBrush(Qt::blue);     // Fill the rectangle with blue color
    }
};

//-----------------------------------------
// GameView is the main window and game controller.
// It handles drawing, physics, input, and level generation.
class GameView : public QGraphicsView {
    Q_OBJECT

public:
    GameView(QGraphicsScene* scene)
        : QGraphicsView(scene), player(new Player()), winCircle(nullptr), deaths(0), level(0), verticalVelocity(0), gameOverText(nullptr) {

        // Set the size of the game window
        setFixedSize(1000, 500);

        // Ensure the window can receive keyboard input
        setFocusPolicy(Qt::StrongFocus);

        // Remove scrollbars (we don't want to scroll around the scene)
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        // Add player to the scene
        scene->addItem(player);

        // Create on-screen text for lives and level
        livesText = new QGraphicsTextItem();
        levelsText = new QGraphicsTextItem();
        scene->addItem(livesText);
        scene->addItem(levelsText);
        livesText->setPos(10, 10);
        levelsText->setPos(10, 30);

        // Start the timer to update the game 60 times per second (1000ms / 16 ≈ 60fps)
        moveTimer = new QTimer(this);
        connect(moveTimer, &QTimer::timeout, this, &GameView::updatePosition);
        moveTimer->start(16);

        // Set the background to light blue
        scene->setBackgroundBrush(QBrush(QColor(173, 216, 230)));

        // Generate the first level
        generateLevel();
    }

protected:
    // Whenever a key is pressed, record it
    void keyPressEvent(QKeyEvent* event) override {
        keysPressed.insert(event->key());
    }

    // Whenever a key is released, stop tracking it
    void keyReleaseEvent(QKeyEvent* event) override {
        keysPressed.remove(event->key());
    }

    // Automatically resize the scene when the window resizes
    void resizeEvent(QResizeEvent* event) override {
        QRectF newRect(0, 0, viewport()->width(), viewport()->height());
        scene()->setSceneRect(newRect);
        QGraphicsView::resizeEvent(event);
    }

private:
    // Game elements
    Player* player;
    QGraphicsEllipseItem* winCircle;                // The yellow goal
    QVector<QGraphicsPolygonItem*> redTriangles;    // Spikes that kill the player
    QVector<QGraphicsRectItem*> platforms;          // Platforms the player stands on
    QTimer* moveTimer;                              // The game loop
    QSet<int> keysPressed;                          // Set of currently pressed keys
    int verticalVelocity;                           // Used for jumping and falling
    int deaths;                                     // Number of times the player hit a spike
    int level;                                      // Number of levels completed
    QGraphicsTextItem* livesText;                   // HUD text
    QGraphicsTextItem* levelsText;
    QPointF lastSpawnPos;                           // Where to respawn the player
    QGraphicsTextItem* gameOverText;                // Text shown on game over

    //-----------------------------------------
    // This function builds or resets the level layout
    void generateLevel() {
        // Remove the previous win circle and delete it
        if (winCircle) scene()->removeItem(winCircle);
        delete winCircle;

        // Remove all old spikes
        for (auto tri : redTriangles) scene()->removeItem(tri);
        redTriangles.clear();

        // Remove all old platforms
        for (auto plt : platforms) scene()->removeItem(plt);
        platforms.clear();

        // Remove the game over screen if it's still showing
        if (gameOverText) {
            scene()->removeItem(gameOverText);
            delete gameOverText;
            gameOverText = nullptr;
        }

        // Set the player starting point
        QRectF sceneBounds = scene()->sceneRect();
        QPointF spawnPos;

        if (level == 0 || lastSpawnPos.isNull()) {
            // First level: put the player in the middle bottom
            spawnPos = QPointF(sceneBounds.width() / 2, sceneBounds.height() - player->rect().height());
            lastSpawnPos = spawnPos;

            // Create the goal near the bottom
            winCircle = new QGraphicsEllipseItem(sceneBounds.width() / 2 - 100, sceneBounds.height() - 50, 30, 30);

            // Create a platform directly under spawn
            QGraphicsRectItem* basePlatform = new QGraphicsRectItem(spawnPos.x() - 40, spawnPos.y() + 20, 100, 10);
            basePlatform->setBrush(Qt::darkGray);
            scene()->addItem(basePlatform);
            platforms.append(basePlatform);
        } else {
            // For new levels: generate a random goal position far from the player
            spawnPos = lastSpawnPos;
            QPointF winPos;

            do {
                winPos = QPointF(QRandomGenerator::global()->bounded(sceneBounds.width()),
                                 QRandomGenerator::global()->bounded(sceneBounds.height() / 2));
            } while (QLineF(spawnPos, winPos).length() < 150); // ensure goal is not too close

            winCircle = new QGraphicsEllipseItem(winPos.x(), winPos.y(), 30, 30);

            // Generate platforms from spawn to win position
            int numPlatforms = 14;
            qreal stepY = (spawnPos.y() - winPos.y()) / numPlatforms;
            for (int i = 0; i < numPlatforms; ++i) {
                qreal y = spawnPos.y() - i * stepY;
                qreal x;
                do {
                    x = QRandomGenerator::global()->bounded(sceneBounds.width() - 80);
                } while (qAbs(x - spawnPos.x()) < 100 && qAbs(y - spawnPos.y()) < 80);

                QGraphicsRectItem* platform = new QGraphicsRectItem(x, y, 80, 10);
                platform->setBrush(Qt::darkGray);
                scene()->addItem(platform);
                platforms.append(platform);

                // 40% chance to spawn a red triangle (spike) on the platform
                if (QRandomGenerator::global()->bounded(100) < 40) {
                    int spikeOffset = QRandomGenerator::global()->bounded(10, 70);
                    QPolygonF triangle;
                    triangle << QPointF(x + spikeOffset + 10, y - 10)
                             << QPointF(x + spikeOffset, y)
                             << QPointF(x + spikeOffset + 20, y);
                    QGraphicsPolygonItem* tri = new QGraphicsPolygonItem(triangle);
                    tri->setBrush(Qt::red);
                    scene()->addItem(tri);
                    redTriangles.append(tri);
                }
            }

            // Add a few platforms near the goal to make landing easier
            int safePlatforms = QRandomGenerator::global()->bounded(2, 4);
            for (int i = 0; i < safePlatforms; ++i) {
                qreal px = winPos.x() + QRandomGenerator::global()->bounded(-60, 60);
                qreal py = winPos.y() + 40 + QRandomGenerator::global()->bounded(0, 40);
                px = qBound(0.0, px, sceneBounds.width() - 80);
                py = qBound(0.0, py, sceneBounds.height() - 10);

                QGraphicsRectItem* platform = new QGraphicsRectItem(px, py, 80, 10);
                platform->setBrush(Qt::darkGray);
                scene()->addItem(platform);
                platforms.append(platform);
            }
        }

        // Add the player and win circle to the scene
        player->setPos(spawnPos);
        scene()->addItem(winCircle);
        winCircle->setBrush(Qt::yellow);

        // Update the heads-up display text
        updateHUD();
    }

    //-----------------------------------------
    // Updates the "Lives left" and "Levels won" text
    void updateHUD() {
        livesText->setPlainText(QString("Lives left: %1").arg(10 - deaths));
        levelsText->setPlainText(QString("Levels won: %1").arg(level));
    }

    //-----------------------------------------
    // Show a red "Game Over" message in the center
    void showGameOver() {
        gameOverText = new QGraphicsTextItem();
        gameOverText->setPlainText(QString("Game Over!\nYou passed %1 levels.").arg(level));
        QFont font;
        font.setPointSize(24);
        font.setBold(true);
        gameOverText->setFont(font);
        gameOverText->setDefaultTextColor(Qt::red);
        gameOverText->setPos(scene()->width() / 2 - 150, scene()->height() / 2 - 50);
        scene()->addItem(gameOverText);
    }

private slots:
    //-----------------------------------------
    // The main game loop, called ~60 times per second
    void updatePosition() {
        // Stop the game if the player has died too many times
        if (deaths >= 10) {
            if (!gameOverText) {
                showGameOver();
            }
            return;
        }

        // Store the player's current position and size
        QPointF currentPos = player->pos();
        QRectF sceneBounds = scene()->sceneRect();
        QRectF playerRect = player->boundingRect();

        // Move left and right
        if (keysPressed.contains(Qt::Key_D)) currentPos.setX(currentPos.x() + 7);
        if (keysPressed.contains(Qt::Key_A)) currentPos.setX(currentPos.x() - 7);

        // Simulate gravity by reducing upward velocity
        verticalVelocity -= 1;
        QPointF nextPos = currentPos;
        nextPos.setY(currentPos.y() - verticalVelocity); // Y is inverted in Qt

        bool onGround = false;

        // Check for collision with any platform
        for (auto* platform : platforms) {
            QRectF platRect = platform->sceneBoundingRect();
            QRectF playerNextRect(nextPos, playerRect.size());

            // Only land if falling down and above the platform
            if (playerNextRect.intersects(platRect) && verticalVelocity <= 0 &&
                currentPos.y() + playerRect.height() <= platRect.top()) {
                nextPos.setY(platRect.top() - playerRect.height());
                verticalVelocity = 0;
                onGround = true;
                break;
            }
        }

        // Stop the fall if player hits the bottom of the scene
        if (nextPos.y() >= sceneBounds.bottom() - playerRect.height()) {
            nextPos.setY(sceneBounds.bottom() - playerRect.height());
            onGround = true;
            verticalVelocity = 0;
        }

        // Jump when pressing W while on ground
        if (keysPressed.contains(Qt::Key_W) && onGround) {
            verticalVelocity = 20;
        }

        // Prevent the player from leaving screen horizontally
        nextPos.setX(qBound(sceneBounds.left(), nextPos.x(), sceneBounds.right() - playerRect.width()));
        player->setPos(nextPos);

        // Check for winning
        if (player->collidesWithItem(winCircle)) {
            level++;
            generateLevel();
        }

        // Check for hitting a spike
        for (auto* tri : redTriangles) {
            if (player->collidesWithItem(tri)) {
                deaths++;
                player->setPos(lastSpawnPos);
                break;
            }
        }

        // Update UI text
        updateHUD();
    }
};

//-----------------------------------------
// The main function that runs the whole application
int main(int argc, char *argv[]) {
    QApplication app(argc, argv); // Create the Qt app

    QGraphicsScene scene;
    scene.setSceneRect(0, 0, 1000, 500); // Set game world size

    GameView view(&scene); // Create and show the game
    view.show();

    return app.exec(); // Start the event loop
}

#include "main.moc" // Required if using Q_OBJECT in the same file
