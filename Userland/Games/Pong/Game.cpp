/*
 * Copyright (c) 2020, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Game.h"
#include <AK/Random.h>

namespace Pong {

Game::Game()
{
    set_override_cursor(Gfx::StandardCursor::Hidden);
    start_timer(16);
    reset();
}

Game::~Game()
{
}

void Game::reset_paddles()
{
    m_player1_paddle.moving_up = false;
    m_player1_paddle.moving_down = false;
    m_player1_paddle.rect = { game_width - 12, game_height / 2 - 40, m_player1_paddle.width, m_player1_paddle.height };
    m_player2_paddle.moving_up = false;
    m_player2_paddle.moving_down = false;
    m_player2_paddle.rect = { 4, game_height / 2 - 40, m_player2_paddle.width, m_player2_paddle.height };
}

void Game::reset()
{
    reset_ball(1);
    reset_paddles();
}

void Game::timer_event(Core::TimerEvent&)
{
    tick();
}

void Game::paint_event(GUI::PaintEvent& event)
{
    GUI::Painter painter(*this);
    painter.add_clip_rect(event.rect());

    painter.fill_rect(rect(), Color::Black);
    painter.fill_rect(enclosing_int_rect(m_net.rect()), m_net.color);

    painter.fill_ellipse(enclosing_int_rect(m_ball.rect()), Color::Red);

    painter.fill_rect(enclosing_int_rect(m_player1_paddle.rect), m_player1_paddle.color);
    painter.fill_rect(enclosing_int_rect(m_player2_paddle.rect), m_player2_paddle.color);

    painter.draw_text(player_1_score_rect(), String::formatted("{}", m_player_1_score), Gfx::TextAlignment::TopLeft, Color::White);
    painter.draw_text(player_2_score_rect(), String::formatted("{}", m_player_2_score), Gfx::TextAlignment::TopLeft, Color::White);
}

void Game::keyup_event(GUI::KeyEvent& event)
{
    switch (event.key()) {
    case Key_Up:
        m_player1_paddle.moving_up = false;
        break;
    case Key_Down:
        m_player1_paddle.moving_down = false;
        break;
    default:
        break;
    }
}

void Game::keydown_event(GUI::KeyEvent& event)
{
    switch (event.key()) {
    case Key_Escape:
        GUI::Application::the()->quit();
        break;
    case Key_Up:
        m_player1_paddle.moving_up = true;
        break;
    case Key_Down:
        m_player1_paddle.moving_down = true;
        break;
    default:
        break;
    }
}

void Game::mousemove_event(GUI::MouseEvent& event)
{
    float new_paddle_y = event.y() - m_player1_paddle.rect.height() / 2;
    new_paddle_y = max(0.0f, new_paddle_y);
    new_paddle_y = min(game_height - m_player1_paddle.rect.height(), new_paddle_y);
    m_player1_paddle.rect.set_y(new_paddle_y);
}

void Game::reset_ball(int serve_to_player)
{
    int position_y_min = (game_width / 2) - 50;
    int position_y_max = (game_width / 2) + 50;
    int position_y = get_random<u32>() % (position_y_max - position_y_min + 1) + position_y_min;
    int position_x = (game_height / 2);
    int velocity_y = get_random<u32>() % 3 + 1;
    int velocity_x = 5 + (5 - velocity_y);
    if (get_random<u32>() % 2)
        velocity_y = velocity_y * -1;
    if (serve_to_player == 2)
        velocity_x = velocity_x * -1;

    m_ball = {};
    m_ball.position = { position_x, position_y };
    m_ball.velocity = { velocity_x, velocity_y };
}

void Game::game_over(int winner)
{
    GUI::MessageBox::show(window(), String::formatted("Player {} wins!", winner), "Pong", GUI::MessageBox::Type::Warning, GUI::MessageBox::InputType::OK);
}

void Game::round_over(int winner)
{
    stop_timer();
    if (winner == 1)
        m_player_1_score++;

    if (winner == 2)
        m_player_2_score++;

    if (m_player_1_score == m_score_to_win || m_player_2_score == m_score_to_win) {
        game_over(winner);
        return;
    }

    reset_ball(winner);
    reset_paddles();
    start_timer(16);
}

void Game::calculate_move()
{
    if ((m_ball.y() + m_ball.radius) < (m_player2_paddle.rect.y() + (m_player2_paddle.rect.height() / 2))) {
        m_player2_paddle.moving_up = true;
        m_player2_paddle.moving_down = false;
        return;
    }
    if ((m_ball.y() + m_ball.radius) > (m_player2_paddle.rect.y() + (m_player2_paddle.rect.height() / 2))) {
        m_player2_paddle.moving_up = false;
        m_player2_paddle.moving_down = true;
        return;
    }
    m_player2_paddle.moving_up = false;
    m_player2_paddle.moving_down = false;
}

void Game::tick()
{
    auto new_ball = m_ball;
    new_ball.position += new_ball.velocity;

    if (new_ball.y() < new_ball.radius || new_ball.y() > game_height - new_ball.radius) {
        new_ball.position.set_y(m_ball.y());
        new_ball.velocity.set_y(new_ball.velocity.y() * -1);
    }

    if (new_ball.x() < new_ball.radius) {
        round_over(1);
        return;
    }

    if (new_ball.x() > (game_width - new_ball.radius)) {
        round_over(2);
        return;
    }

    if (new_ball.rect().intersects(m_player1_paddle.rect)) {
        new_ball.position.set_x(m_ball.x());
        new_ball.velocity.set_x(new_ball.velocity.x() * -1);

        float distance_to_middle_of_paddle = new_ball.y() - m_player1_paddle.rect.center().y();
        float relative_impact_point = distance_to_middle_of_paddle / m_player1_paddle.rect.height();
        new_ball.velocity.set_y(relative_impact_point * 7);
    }

    if (new_ball.rect().intersects(m_player2_paddle.rect)) {
        new_ball.position.set_x(m_ball.x());
        new_ball.velocity.set_x(new_ball.velocity.x() * -1);

        float distance_to_middle_of_paddle = new_ball.y() - m_player2_paddle.rect.center().y();
        float relative_impact_point = distance_to_middle_of_paddle / m_player2_paddle.rect.height();
        new_ball.velocity.set_y(relative_impact_point * 7);
    }

    if (m_player1_paddle.moving_up) {
        m_player1_paddle.rect.set_y(max(0.0f, m_player1_paddle.rect.y() - m_player1_paddle.speed));
    }
    if (m_player1_paddle.moving_down) {
        m_player1_paddle.rect.set_y(min(game_height - m_player1_paddle.rect.height(), m_player1_paddle.rect.y() + m_player1_paddle.speed));
    }

    calculate_move();

    if (m_player2_paddle.moving_up) {
        m_player2_paddle.rect.set_y(max(0.0f, m_player2_paddle.rect.y() - m_player2_paddle.speed));
    }
    if (m_player2_paddle.moving_down) {
        m_player2_paddle.rect.set_y(min(game_height - m_player2_paddle.rect.height(), m_player2_paddle.rect.y() + m_player2_paddle.speed));
    }

    m_ball = new_ball;

    update();
}

}
