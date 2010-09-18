
/*  Copyright (c) 2010, Patrick Tierney
 *
 *  This file is part of INC (INC's Not CAD).
 *
 *  INC is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  INC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with INC.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <vector>

#include <cinder/gl/gl.h>
#include <cinder/gl/Texture.h>
#include <cinder/Ray.h>
#include <cinder/BSpline.h>
#include <cinder/Vector.h>
#include <cinder/app/MouseEvent.h>
#include <cinder/Surface.h>

#include <inc/Module.h>

namespace inc {

class ControlPoint;

class CurveSketcher : public Module {
public:
    CurveSketcher();
    virtual ~CurveSketcher();

    // Module override methods
    void setup();
    void update();
    void draw();

    // when the mouse is pressed and released without dragging
    void on_mouse_click(ci::Ray);

    // callback for the button press
    bool activate_button_pressed(bool);

    std::tr1::shared_ptr<ci::BSpline3f> current_spline();

    static CurveSketcher& instance();

private:
    void set_up_sketcher();
    void finish_sketcher();
    void generate_spline(bool closed); // called once per click
    void draw_control_points();

    static CurveSketcher* instance_;
    bool active_;
    std::tr1::shared_ptr<ci::BSpline3f> current_spline_;
    std::vector<ci::Vec3f> points_;

    // specified by position and normal
    ci::Ray drawing_plane_;
    int degree_;

    // the spline is defined between 0 and 1
    float rendering_resolution_;
    float line_thickness_;
    ci::ColorA line_color_;

    // there are as many control points as there are points_
    std::vector<std::tr1::shared_ptr<ControlPoint> > control_points_;
};


class ControlPoint {
public:
    ControlPoint(ci::Vec3f pos);

    virtual void setup();
    // this draws the billboarded circle
    virtual void draw();

    bool mouse_pressed(ci::Ray);
    bool mouse_dragged(ci::app::MouseEvent);
    bool mouse_released(ci::app::MouseEvent);

    // dircetions of the control points
    enum Direction {
        X_INCREASE,
        X_DECREASE,
        Z_INCREASE,
        Z_DECREASE
    };

protected:
    // called when there's been a sucessful drag
    //virtual void move_point(ci::app::MouseEvent evt) = 0;

    ci::Vec3f position_;
    bool active_;
    float arrow_size_;

    float position_image_dim_;
    ci::Surface position_image_;
    ci::gl::Texture position_texture_;
};

/*
class XIntreaseControlPoint : public ControlPoint {

};
*/



}