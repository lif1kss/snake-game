/*
 * Игра «Змейка» на C++ с графикой SFML.
 *
 * Как устроена программа (для новичка):
 * 1) main() — точка входа: создаёт игру и запускает цикл.
 * 2) Класс SnakeGame — вся логика: поле, змейка, еда, счёт, окно.
 * 3) Каждый кадр: обработать клавиши -> обновить позиции -> нарисовать картинку.
 *
 * Поле — это сетка клеток. Координаты (x, y) — номера клетки, не пиксели на экране.
 * Пиксели считаются отдельно: клетка 20x20 пикселей.
 */

// --- Подключение библиотек ---
#include <SFML/Graphics.hpp> // SFML: окно, прямоугольники, текст, клавиатура, таймер
#include <cstdlib>           // rand/srand — случайные числа (для появления еды)
#include <ctime>             // time — «зерно» для rand, чтобы еда была в разных местах
#include <fstream>           // чтение/запись файла (рекорд сохраняется на диск)
#include <optional>          // optional — «может быть значение, а может нет» (для текста)
#include <string>            // строки для надписей Score / Best

// Одна клетка на поле: пара координат (столбец x, строка y).
struct Point {
    int x{}; // горизонталь: 0 — левый край
    int y{}; // вертикаль: 0 — верхний край
};

// Куда движется голова. enum class — набор именованных констант.
enum class Direction {
    Up,
    Down,
    Left,
    Right
};

// Весь код игры собран в одном классе — так проще держать данные и функции вместе.
class SnakeGame {
public:
    // Конструктор вызывается один раз при создании объекта game в main().
    // gridWidth, gridHeight — размер поля в клетках (например 30 на 20).
    SnakeGame(int gridWidth, int gridHeight)
        : gridWidth_(gridWidth),
          gridHeight_(gridHeight),
          // Создаём окно: ширина и высота в пикселях считаются в pixelWidth/Height().
          window_(sf::VideoMode({static_cast<unsigned>(pixelWidth()),
                                 static_cast<unsigned>(pixelHeight())}),
                  "Snake SFML"),
          tickClock_() {
        window_.setFramerateLimit(60); // не рисовать чаще 60 раз в секунду
        loadBestScore();               // прочитать рекорд из файла, если есть
        loadFont();                    // шрифт для текста на экране
        setupHudText();                // подготовить надписи (счёт, подсказки)
        std::srand(static_cast<unsigned>(std::time(nullptr))); // инициализация random
        reset();                       // начальная змейка, еда, счёт = 0
    }

    // Главный цикл — сердце программы. Работает, пока окно открыто.
    void run() {
        while (window_.isOpen()) {
            handleEvents(); // закрытие окна, нажатия клавиш

            // Если игрок проиграл — только рисуем экран и ждём R (рестарт).
            if (!isRunning_) {
                draw();
                continue; // переходим к следующей итерации while, update() не вызываем
            }

            // Змейка двигается не каждый кадр отрисовки, а с задержкой speedMs_ мс.
            if (shouldMoveThisFrame()) {
                update(); // один шаг змейки: новая голова, проверка столкновений
            }

            draw(); // показать всё на экране
        }
    }

private:
    // --- Константы (не меняются во время игры) ---
    static const int kMaxSnakeLength = 800; // максимум сегментов в массиве
    static const int kCellSize = 20;        // размер одной клетки в пикселях
    static const int kHudHeight = 70;       // высота верхней панели со счётом
    static const int kBoostDivisor = 2;     // Shift ускоряет в 2 раза (задержка / 2)
    static const char* kRecordFileName;     // имя файла рекорда

    // --- Состояние игры ---
    int gridWidth_;              // ширина поля в клетках
    int gridHeight_;             // высота поля в клетках
    int score_{0};               // очки в текущей партии
    int bestScore_{0};           // лучший результат за все запуски
    int speedMs_{130};           // пауза между шагами змейки (мс); меньше = быстрее
    bool isRunning_{true};       // true — игра идёт, false — game over
    Direction direction_{Direction::Right}; // куда едет голова сейчас

    // Змейка — массив точек. snake_[0] всегда голова, дальше — тело и хвост.
    Point snake_[kMaxSnakeLength]{};
    int snakeLength_{0}; // сколько сегментов реально занято (3 в начале)
    Point food_{};       // где лежит еда

    // --- Графика SFML ---
    sf::RenderWindow window_; // само окно
    sf::Font font_;           // шрифт для текста
    bool hasFont_{false};     // удалось ли загрузить шрифт

    // optional — текст создаём только если шрифт загрузился (в SFML 3 Text требует Font).
    std::optional<sf::Text> scoreText_;
    std::optional<sf::Text> bestText_;
    std::optional<sf::Text> hintText_;
    std::optional<sf::Text> gameOverText_;

    sf::Clock tickClock_;              // секундомер с момента старта/рестарта
    sf::Time lastMoveTime_{sf::Time::Zero}; // когда змейка делала последний шаг

    // Ширина окна в пикселях = клетки * размер клетки.
    int pixelWidth() const {
        return gridWidth_ * kCellSize;
    }

    // Высота = поле + верхняя панель HUD.
    int pixelHeight() const {
        return gridHeight_ * kCellSize + kHudHeight;
    }

    // Перевод координат клетки (x,y) в позицию левого верхнего угла прямоугольника на экране.
    sf::Vector2f cellTopLeft(int x, int y) const {
        return sf::Vector2f(
            static_cast<float>(x * kCellSize),
            static_cast<float>(y * kCellSize + kHudHeight)); // поле ниже панели HUD
    }

    // Пытаемся открыть системный шрифт — без него текст не покажется.
    void loadFont() {
        const char* fontPaths[] = {
            "C:/Windows/Fonts/arial.ttf",
            "C:/Windows/Fonts/Arial.ttf",
            "fonts/arial.ttf"
        };

        for (const char* path : fontPaths) {
            if (font_.openFromFile(path)) {
                hasFont_ = true;
                return;
            }
        }
        hasFont_ = false;
    }

    // HUD = Head-Up Display — надписи поверх игры (счёт, подсказки).
    void setupHudText() {
        if (!hasFont_) {
            return;
        }

        scoreText_.emplace(font_, "", 20);
        scoreText_->setFillColor(sf::Color::White);
        scoreText_->setPosition(sf::Vector2f{10.f, 8.f});

        bestText_.emplace(font_, "", 20);
        bestText_->setFillColor(sf::Color(200, 200, 200));
        bestText_->setPosition(sf::Vector2f{10.f, 34.f});

        hintText_.emplace(font_, "WASD / arrows | Shift boost | R restart | Esc quit", 16);
        hintText_->setFillColor(sf::Color(180, 220, 180));
        hintText_->setPosition(sf::Vector2f{10.f, static_cast<float>(pixelHeight() - 24)});

        gameOverText_.emplace(font_, "Game Over! Press R to restart", 28);
        gameOverText_->setFillColor(sf::Color(255, 90, 90));
        gameOverText_->setPosition(sf::Vector2f{
            static_cast<float>(pixelWidth() / 2 - 220),
            static_cast<float>(pixelHeight() / 2)});
    }

    // Читаем рекорд из текстового файла (одно число).
    void loadBestScore() {
        std::ifstream inFile(kRecordFileName);
        if (!(inFile >> bestScore_)) {
            bestScore_ = 0; // файла нет или он пустой
        }
    }

    void saveBestScore() const {
        std::ofstream outFile(kRecordFileName, std::ios::trunc);
        if (outFile) {
            outFile << bestScore_;
        }
    }

    // После смерти: если набрали больше рекорда — запоминаем.
    void updateBestScoreIfNeeded() {
        if (score_ > bestScore_) {
            bestScore_ = score_;
            saveBestScore();
        }
    }

    // Зажат ли Shift прямо сейчас (проверяем каждый кадр).
    bool isShiftPressed() const {
        return sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
               sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
    }

    // Сколько миллисекунд ждать до следующего шага змейки.
    int currentFrameDelayMs() const {
        if (!isShiftPressed()) {
            return speedMs_;
        }
        int boostedDelay = speedMs_ / kBoostDivisor;
        if (boostedDelay < 20) {
            boostedDelay = 20; // слишком быстро играть нельзя
        }
        return boostedDelay;
    }

    // Пора ли сдвинуть змейку? Сравниваем таймер с задержкой speedMs_.
    bool shouldMoveThisFrame() {
        const sf::Time delay = sf::milliseconds(currentFrameDelayMs());
        if (tickClock_.getElapsedTime() - lastMoveTime_ >= delay) {
            lastMoveTime_ = tickClock_.getElapsedTime();
            return true;
        }
        return false;
    }

    // Новая партия или рестарт после поражения.
    void reset() {
        score_ = 0;
        speedMs_ = 130;
        isRunning_ = true;
        direction_ = Direction::Right;
        lastMoveTime_ = sf::Time::Zero;
        tickClock_.restart();

        const int startX = gridWidth_ / 2;
        const int startY = gridHeight_ / 2;

        snakeLength_ = 3;
        snake_[0] = {startX, startY};         // голова в центре
        snake_[1] = {startX - 1, startY};     // сегмент слева
        snake_[2] = {startX - 2, startY};     // хвост ещё левее

        spawnFood();
        updateHudStrings();
    }

    void updateHudStrings() {
        if (!hasFont_) {
            return;
        }
        scoreText_->setString("Score: " + std::to_string(score_));
        bestText_->setString("Best: " + std::to_string(bestScore_));
    }

    // SFML складывает события в очередь: закрытие окна, нажатия клавиш и т.д.
    void handleEvents() {
        while (const std::optional<sf::Event> event = window_.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window_.close();
                return;
            }

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                handleKeyPressed(keyPressed->code);
            }
        }
    }

    void handleKeyPressed(sf::Keyboard::Key key) {
        if (key == sf::Keyboard::Key::Escape) {
            window_.close();
            return;
        }

        if (!isRunning_) {
            if (key == sf::Keyboard::Key::R) {
                reset();
            }
            return;
        }

        // Нельзя развернуться на 180° за один ход (иначе сразу врежемся в себя).
        if ((key == sf::Keyboard::Key::W || key == sf::Keyboard::Key::Up) &&
            direction_ != Direction::Down) {
            direction_ = Direction::Up;
        } else if ((key == sf::Keyboard::Key::S || key == sf::Keyboard::Key::Down) &&
                   direction_ != Direction::Up) {
            direction_ = Direction::Down;
        } else if ((key == sf::Keyboard::Key::A || key == sf::Keyboard::Key::Left) &&
                   direction_ != Direction::Right) {
            direction_ = Direction::Left;
        } else if ((key == sf::Keyboard::Key::D || key == sf::Keyboard::Key::Right) &&
                   direction_ != Direction::Left) {
            direction_ = Direction::Right;
        }
    }

    // Каждый сегмент (кроме головы) копирует координаты предыдущего — «хвост тянется».
    void shiftBodyForward() {
        for (int i = snakeLength_ - 1; i > 0; --i) {
            snake_[i] = snake_[i - 1];
        }
    }

    // Есть ли в точке p сегмент змейки? includeHead=false — не считаем голову (для столкновения).
    bool isSnakeBody(const Point& p, bool includeHead) const {
        const int start = includeHead ? 0 : 1;
        for (int i = start; i < snakeLength_; ++i) {
            if (snake_[i].x == p.x && snake_[i].y == p.y) {
                return true;
            }
        }
        return false;
    }

    // Случайная свободная клетка для еды. do-while повторяет, пока не найдём пустую.
    void spawnFood() {
        Point p{};
        do {
            p.x = 1 + std::rand() % (gridWidth_ - 2);
            p.y = 1 + std::rand() % (gridHeight_ - 2);
        } while (isSnakeBody(p, true));
        food_ = p;
    }

    // Один игровой тик: новая позиция головы, столкновения, рост, сдвиг тела.
    void update() {
        Point newHead = snake_[0]; // копия текущей головы

        switch (direction_) {
            case Direction::Up:
                --newHead.y;
                break;
            case Direction::Down:
                ++newHead.y;
                break;
            case Direction::Left:
                --newHead.x;
                break;
            case Direction::Right:
                ++newHead.x;
                break;
        }

        // Края 0 и width-1 / height-1 — стены (рамка поля).
        if (newHead.x <= 0 || newHead.x >= gridWidth_ - 1 ||
            newHead.y <= 0 || newHead.y >= gridHeight_ - 1 ||
            isSnakeBody(newHead, false)) {
            updateBestScoreIfNeeded();
            isRunning_ = false;
            updateHudStrings();
            return;
        }

        const bool ateFood = (newHead.x == food_.x && newHead.y == food_.y);

        if (ateFood && snakeLength_ < kMaxSnakeLength) {
            ++snakeLength_; // длина +1: хвост не удалим на этом шаге
            score_ += 10;
            if (speedMs_ > 70) {
                speedMs_ -= 3; // игра чуть ускоряется
            }
            updateHudStrings();
        }

        shiftBodyForward();
        snake_[0] = newHead; // ставим голову на новое место

        if (ateFood) {
            spawnFood();
        }
    }

    // Рисуем одну клетку цветным квадратом (RectangleShape).
    void drawCell(int x, int y, const sf::Color& color) {
        sf::RectangleShape cell(
            sf::Vector2f(static_cast<float>(kCellSize - 2),
                         static_cast<float>(kCellSize - 2)));
        const sf::Vector2f pos = cellTopLeft(x, y);
        cell.setPosition(sf::Vector2f{pos.x + 1.f, pos.y + 1.f});
        cell.setFillColor(color);
        window_.draw(cell);
    }

    // Двойной цикл по всем клеткам: край — стена, внутри — фон.
    void drawGrid() {
        for (int y = 0; y < gridHeight_; ++y) {
            for (int x = 0; x < gridWidth_; ++x) {
                if (x == 0 || x == gridWidth_ - 1 || y == 0 || y == gridHeight_ - 1) {
                    drawCell(x, y, sf::Color(70, 70, 90));
                } else {
                    drawCell(x, y, sf::Color(30, 45, 35));
                }
            }
        }
    }

    void drawSnake() {
        for (int i = 0; i < snakeLength_; ++i) {
            const sf::Color color = (i == 0) ? sf::Color(80, 220, 120)  // голова ярче
                                             : sf::Color(50, 170, 90);
            drawCell(snake_[i].x, snake_[i].y, color);
        }
    }

    void drawFood() {
        drawCell(food_.x, food_.y, sf::Color(230, 70, 70));
    }

    void drawHudPanel() {
        sf::RectangleShape panel(sf::Vector2f(
            static_cast<float>(pixelWidth()),
            static_cast<float>(kHudHeight)));
        panel.setPosition(sf::Vector2f{0.f, 0.f});
        panel.setFillColor(sf::Color(25, 30, 40));
        window_.draw(panel);
    }

    // Один кадр отрисовки: очистить -> нарисовать всё -> показать на экране (display).
    void draw() {
        window_.clear(sf::Color(20, 28, 24));

        drawHudPanel();
        drawGrid();
        drawFood();
        drawSnake();

        if (hasFont_) {
            window_.draw(*scoreText_);
            window_.draw(*bestText_);
            window_.draw(*hintText_);

            if (!isRunning_) {
                window_.draw(*gameOverText_);
            }
        }

        window_.display(); // буфер становится видимым игроку
    }
};

// Статическая константа класса — задаём значение здесь, вне класса.
const char* SnakeGame::kRecordFileName = "snake_record.txt";

// Программа начинается здесь. Создаём объект game и вызываем run().
int main() {
    SnakeGame game(30, 20); // поле 30 клеток в ширину, 20 в высоту
    game.run();
    return 0; // 0 — нормальное завершение для ОС
}
