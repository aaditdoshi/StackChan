/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "default.h"

using namespace uitk;
using namespace uitk::lvgl_cpp;
using namespace stackchan::avatar;

// Thin horizontal bar sitting above the eye container.
// Eye containers are centered at x=±70, y=-16 relative to the 320×240 panel center.
// We place eyebrows ~24 px above the eye center → y = -40.
static const int      _brow_width       = 26;
static const int      _brow_height      = 5;
static const int      _brow_radius      = 3;
static const Vector2i _brow_base_pos    = Vector2i(-70, -40);  // left; right is mirrored in x
static const Vector2i _brow_min_offset  = Vector2i(-12, -12);
static const Vector2i _brow_max_offset  = Vector2i(12, 12);

DefaultEyebrows::DefaultEyebrows(lv_obj_t* parent, lv_color_t primaryColor, lv_color_t secondaryColor,
                                 bool isLeftEyebrow)
{
    _is_left_eyebrow = isLeftEyebrow;

    _brow = std::make_unique<Container>(parent);
    _brow->setSize(_brow_width, _brow_height);
    _brow->setAlign(LV_ALIGN_CENTER);
    _brow->setBorderWidth(0);
    _brow->setBgColor(primaryColor);
    _brow->setRadius(_brow_radius);
    _brow->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _brow->setTransformPivot(_brow_width / 2, _brow_height / 2);

    setPosition(_position);
    setWeight(100);
    setRotation(0);
}

DefaultEyebrows::~DefaultEyebrows()
{
    _brow.reset();
}

void DefaultEyebrows::setPosition(const Vector2i& position)
{
    Element::setPosition(position);

    auto pos_x = _is_left_eyebrow ? _brow_base_pos.x : -_brow_base_pos.x;
    pos_x += map_range(_position.x, -100, 100, _brow_min_offset.x, _brow_max_offset.x);
    auto pos_y = _brow_base_pos.y + _emotion_y_offset
                 + map_range(_position.y, -100, 100, _brow_min_offset.y, _brow_max_offset.y);

    _brow->setPos(pos_x, pos_y);
}

void DefaultEyebrows::setWeight(int weight)
{
    Feature::setWeight(weight);
    // Weight controls opacity so modifiers can fade eyebrows in/out.
    lv_obj_set_style_opa(_brow->get(), (lv_opa_t)map_range(_weight, 0, 100, (int)LV_OPA_TRANSP, (int)LV_OPA_COVER), 0);
}

void DefaultEyebrows::setRotation(int rotation)
{
    Element::setRotation(rotation);
    _brow->setRotation(rotation);
}

void DefaultEyebrows::setEmotion(const Emotion& emotion)
{
    if (getIgnoreEmotion()) {
        return;
    }

    // rotation > 0 → CW (inner end drops for left brow).
    // Right brow gets negated → CCW (inner end drops on its side) → matching angry V.
    // y_offset > 0 → brow shifts down; < 0 → shifts up.
    struct Style {
        int rotation;
        int y_offset;
    };

    Style s;
    switch (emotion) {
        case Emotion::Neutral: s = {0, 0};     break;
        case Emotion::Happy:   s = {-100, 0};  break;   // outer arch, slight lift
        case Emotion::Angry:   s = {300, 4};   break;   // inner drops, brows lower
        case Emotion::Sad:     s = {-300, 0};  break;   // inner rises, sad droop
        case Emotion::Doubt:   s = {-200, -6}; break;   // raised + arched
        case Emotion::Sleepy:  s = {100, 3};   break;   // slight droop, lower
        default:               s = {0, 0};     break;
    }

    _emotion_y_offset = s.y_offset;

    // Mirror rotation for right eyebrow (same convention as DefaultEyes).
    if (_is_left_eyebrow) {
        setRotation(s.rotation);
    } else {
        setRotation(-s.rotation);
    }

    // Re-apply position so the y_offset takes effect immediately.
    setPosition(_position);
}

void DefaultEyebrows::setVisible(bool visible)
{
    Element::setVisible(visible);
    _brow->setHidden(!visible);
}
