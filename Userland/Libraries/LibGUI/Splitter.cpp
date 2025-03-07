/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibGUI/BoxLayout.h>
#include <LibGUI/Painter.h>
#include <LibGUI/Splitter.h>
#include <LibGUI/Window.h>
#include <LibGfx/Palette.h>

REGISTER_WIDGET(GUI, HorizontalSplitter)
REGISTER_WIDGET(GUI, VerticalSplitter)

namespace GUI {

Splitter::Splitter(Orientation orientation)
    : m_orientation(orientation)
{
    REGISTER_INT_PROPERTY("first_resizee_minimum_size", first_resizee_minimum_size, set_first_resizee_minimum_size);
    REGISTER_INT_PROPERTY("second_resizee_minimum_size", second_resizee_minimum_size, set_second_resizee_minimum_size);

    set_background_role(ColorRole::Button);
    set_layout<BoxLayout>(orientation);
    set_fill_with_background_color(true);
    layout()->set_spacing(3);
}

Splitter::~Splitter()
{
}

void Splitter::paint_event(PaintEvent& event)
{
    Painter painter(*this);
    painter.add_clip_rect(event.rect());
    painter.fill_rect(m_grabbable_rect, palette().hover_highlight());
}

void Splitter::resize_event(ResizeEvent& event)
{
    Widget::resize_event(event);
    m_grabbable_rect = {};
}

void Splitter::override_cursor(bool do_override)
{
    if (do_override) {
        if (!m_overriding_cursor) {
            set_override_cursor(m_orientation == Orientation::Horizontal ? Gfx::StandardCursor::ResizeColumn : Gfx::StandardCursor::ResizeRow);
            m_overriding_cursor = true;
        }
    } else {
        if (m_overriding_cursor) {
            set_override_cursor(Gfx::StandardCursor::None);
            m_overriding_cursor = false;
        }
    }
}

void Splitter::leave_event(Core::Event&)
{
    if (!m_resizing)
        override_cursor(false);
    if (!m_grabbable_rect.is_empty()) {
        m_grabbable_rect = {};
        update();
    }
}

bool Splitter::get_resize_candidates_at(const Gfx::IntPoint& position, Widget*& first, Widget*& second)
{
    int x_or_y = position.primary_offset_for_orientation(m_orientation);
    Widget* previous_widget = nullptr;
    bool found_candidates = false;
    for_each_child_widget([&](auto& child_widget) {
        if (!child_widget.is_visible()) {
            // We need to skip over widgets that are not visible as they
            // are not necessarily in the correct location (anymore)
            return IterationDecision::Continue;
        }
        if (!previous_widget) {
            previous_widget = &child_widget;
            return IterationDecision::Continue;
        }

        if (x_or_y > previous_widget->content_rect().last_edge_for_orientation(m_orientation)
            && x_or_y <= child_widget.content_rect().first_edge_for_orientation(m_orientation)) {
            first = previous_widget;
            second = &child_widget;
            found_candidates = true;
            return IterationDecision::Break;
        }

        previous_widget = &child_widget;
        return IterationDecision::Continue;
    });
    return found_candidates;
}

void Splitter::mousedown_event(MouseEvent& event)
{
    if (event.button() != MouseButton::Left)
        return;
    m_resizing = true;

    Widget* first { nullptr };
    Widget* second { nullptr };
    if (!get_resize_candidates_at(event.position(), first, second))
        return;

    m_first_resizee = *first;
    m_second_resizee = *second;
    m_first_resizee_start_size = first->size();
    m_second_resizee_start_size = second->size();
    m_resize_origin = event.position();
}

void Splitter::recompute_grabbable_rect(const Widget& first, const Widget& second)
{
    auto first_edge = first.content_rect().primary_offset_for_orientation(m_orientation) + first.content_rect().primary_size_for_orientation(m_orientation);
    auto second_edge = second.content_rect().primary_offset_for_orientation(m_orientation);
    Gfx::IntRect rect;
    rect.set_primary_offset_for_orientation(m_orientation, first_edge);
    rect.set_primary_size_for_orientation(m_orientation, second_edge - first_edge);
    rect.set_secondary_offset_for_orientation(m_orientation, first.content_rect().secondary_offset_for_orientation(m_orientation));
    rect.set_secondary_size_for_orientation(m_orientation, first.content_rect().secondary_size_for_orientation(m_orientation));

    if (m_grabbable_rect != rect) {
        m_grabbable_rect = rect;
        update();
    }
}

void Splitter::mousemove_event(MouseEvent& event)
{
    if (!m_resizing) {
        Widget* first { nullptr };
        Widget* second { nullptr };
        if (!get_resize_candidates_at(event.position(), first, second)) {
            override_cursor(false);
            return;
        }
        recompute_grabbable_rect(*first, *second);
        override_cursor(m_grabbable_rect.contains(event.position()));
        return;
    }
    auto delta = event.position() - m_resize_origin;
    if (!m_first_resizee || !m_second_resizee) {
        // One or both of the resizees were deleted during an ongoing resize, screw this.
        m_resizing = false;
        return;
    }
    auto new_first_resizee_size = m_first_resizee_start_size;
    auto new_second_resizee_size = m_second_resizee_start_size;

    new_first_resizee_size.set_primary_size_for_orientation(m_orientation, new_first_resizee_size.primary_size_for_orientation(m_orientation) + delta.primary_offset_for_orientation(m_orientation));
    new_second_resizee_size.set_primary_size_for_orientation(m_orientation, new_second_resizee_size.primary_size_for_orientation(m_orientation) - delta.primary_offset_for_orientation(m_orientation));

    if (new_first_resizee_size.primary_size_for_orientation(m_orientation) < m_first_resizee_minimum_size) {
        int correction = m_first_resizee_minimum_size - new_first_resizee_size.primary_size_for_orientation(m_orientation);
        new_first_resizee_size.set_primary_size_for_orientation(m_orientation, new_first_resizee_size.primary_size_for_orientation(m_orientation) + correction);
        new_second_resizee_size.set_primary_size_for_orientation(m_orientation, new_second_resizee_size.primary_size_for_orientation(m_orientation) - correction);
    }
    if (new_second_resizee_size.primary_size_for_orientation(m_orientation) < m_second_resizee_minimum_size) {
        int correction = m_second_resizee_minimum_size - new_second_resizee_size.primary_size_for_orientation(m_orientation);
        new_second_resizee_size.set_primary_size_for_orientation(m_orientation, new_second_resizee_size.primary_size_for_orientation(m_orientation) + correction);
        new_first_resizee_size.set_primary_size_for_orientation(m_orientation, new_first_resizee_size.primary_size_for_orientation(m_orientation) - correction);
    }

    if (m_orientation == Orientation::Horizontal) {
        m_first_resizee->set_fixed_width(new_first_resizee_size.width());
        m_second_resizee->set_fixed_width(-1);
    } else {
        m_first_resizee->set_fixed_height(new_first_resizee_size.height());
        m_second_resizee->set_fixed_height(-1);
    }

    invalidate_layout();
}

void Splitter::did_layout()
{
    if (m_first_resizee && m_second_resizee)
        recompute_grabbable_rect(*m_first_resizee, *m_second_resizee);
}

void Splitter::mouseup_event(MouseEvent& event)
{
    if (event.button() != MouseButton::Left)
        return;
    m_resizing = false;
    m_first_resizee = nullptr;
    m_second_resizee = nullptr;
    if (!rect().contains(event.position()))
        set_override_cursor(Gfx::StandardCursor::None);
}

}
