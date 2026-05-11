
#include "affine_pic.h"
#include "gl_canvas.h"
#include "gl_common.h"
#include "lgr.h"
#include "physics_init.h"
#include "LEJATSZO.H"
#include <glad/glad.h>
#include <cmath>
#include <iterator>
#include <string>


static GLuint KuskiShaderProgram;
static GLuint KuskiVAO, KuskiVBO;
static GLuint KuskiUTransform = 0;
static GLuint TexWheel = 0;
static GLuint TexHead = 0;
static GLuint TexBike = 0;
static GLuint TexSusp1 = 0;
static GLuint TexSusp2 = 0;
static GLuint TexPart1 = 0;
static GLuint TexPart2 = 0;
static GLuint TexPart3 = 0;
static GLuint TexPart4 = 0;
static GLuint TexThigh, TexLeg, TexUparm, TexForarm, TexBody;

#define PI 3.1415926535897932
static double BikeFrameX;
static double BikeFrameY;
static vect2 BikeFrameI;
static vect2 BikeFrameJ;
static vect2 BikeFrameR;

static bool StretchEnabled = false;
static double StretchFactor = 1.0;
static vect2 StretchCenter = Vect2i;
static vect2 StretchAxis = Vect2i;



int gl_init_kuski() {

    const char* vert = R"(
    #version 420 core
    layout(std140, binding = 0) uniform GlobalData { vec4 uFrustrum; };
    layout (location = 0) in vec2 pos;
    layout (location = 1) in vec2 texCoord;
    out vec2 fragTexCoord;
    uniform mat3 uTransform;

    void main() {
      vec3 r = uTransform * vec3(pos, 1.0);

      float x = (r.x-uFrustrum.x)/(uFrustrum.z-uFrustrum.x);
      float y = (r.y-uFrustrum.y)/(uFrustrum.w-uFrustrum.y);

      gl_Position = vec4(-1.0 + x * 2.0, -1.0 + y * 2.0, 0.0, 1.0);

      fragTexCoord = texCoord;
    }
    )";

    const char* frag = R"(
    #version 410 core
    in vec2 fragTexCoord;
    out vec4 FragColor;
    uniform sampler2D IndexTexture;
    uniform sampler1D PaletteTexture;
    uniform int tColor;
    uniform vec4 shadowColor;

    void main() {
      float index = texture(IndexTexture, fragTexCoord).r;
      FragColor = texture(PaletteTexture, index);
      if (FragColor.rgb == vec3(1.0, 1.0, 0.0)) {
        FragColor = vec4(0.0);
      //} else if (FragColor.rgb == vec3(0.0)) {
      //  FragColor = vec4(0.0);
      } else if (shadowColor.a != 0.0) {
        FragColor = shadowColor;
      } else {
        //vec4 c = texture(IndexTexture, vec2(fragTexCoord.x, fragTexCoord.y - .05));
        //if (c.a == 0.0) {
        //  FragColor += vec4(.7);
        //}
        //float l = length(fragTexCoord);
        //float fl = floor(l);
        //float fc = fract(l);
        //float r = fract(sin(l * 43758.5453123));
        //float r1 = fract(sin((fl+1.0) * 43758.5453123));
        //float nn = mix(r, r1, fc);
        //FragColor.rgb += vec3(abs(nn));
      }



    }
    )";


    if ((KuskiShaderProgram = gl_shader_program(vert, frag)) == -1) {
        printf("failed to create KuskiShaderProgram\n");
        return -1;
    }

    glGenVertexArrays(1, &KuskiVAO);
    glGenBuffers(1, &KuskiVBO);

    glBindVertexArray(KuskiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, KuskiVBO);

    float quadUnit[24] = {
      0, 0, 0, 0,
      1, 0, 1, 0,
      1, 1, 1, 1,
      0, 0, 0, 0,
      1, 1, 1, 1,
      0, 1, 0, 1
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadUnit), quadUnit, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));

    KuskiUTransform = glGetUniformLocation(KuskiShaderProgram, "uTransform");


    return 0;
}





static void render_part(vect2 u, vect2 v, vect2 r) {

  if (StretchEnabled) {
    // Stretch coordinate r
    double distance = (r - StretchCenter) * StretchAxis;
    vect2 delta = (distance * (1.0 - StretchFactor)) * StretchAxis;
    r = r - delta;

    // Stretch coordinate u
    distance = u * StretchAxis;
    delta = (distance * (1.0 - StretchFactor)) * StretchAxis;
    u = u - delta;

    // Stretch coordinate v
    distance = v * StretchAxis;
    delta = (distance * (1.0 - StretchFactor)) * StretchAxis;
    v = v - delta;
  }

  //float mat3[9] = {
  //  float(2), float(0), 0.0f,
  //  float(0), float(2), 0.0f,
  //  float(0), float(0), 1.0f
  //};
  float mat3[9] = {
    float(u.x), float(u.y), 0.0f,
    float(v.x), float(v.y), 0.0f,
    float(r.x), float(r.y), 1.0f
  };
  glUniformMatrix3fv(KuskiUTransform, 1, false, mat3);


  // It's fast to draw for each part because static buffer
  // In theory it could be one draw call with many textures and an array of mat3s but
  // prob not even worth it
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

static void render_frame_part(GLuint tex, bike_box* box) {

  vect2 u = BikeFrameI * (box->x2 - box->x1);
  vect2 v = BikeFrameJ * (box->y1 - box->y2);
  vect2 r = BikeFrameI * (box->x1 + 260 - BikeFrameX) +
            BikeFrameJ * (BikeFrameY - (box->y1 + 260)) + BikeFrameR;

  glBindTexture(GL_TEXTURE_2D, tex);
  render_part(u, v, r);
}

// Render an affine_pic (remember all affine_pic images are loaded sideways in the lgr)
// All units are in meters
// a = coordinate of middle left of affine_pic position (distal end of the limb)
// b = coordinate of middle right of affine_pic position (proximal end of the limb)
// Along the axis of the vector b->a, displace coordinate a by `a_stretch` meters
// Along the axis of the vector a->b, displace coordinate b by `b_stretch` meters
// height represents the vertical length of the affine_pic (thickness of the limb)
static void render_body_part(GLuint tex, vect2 a, vect2 b, double height,
                              double a_stretch, double b_stretch, bool flip) {


  vect2 i = unit_vector(b - a);
  b = b + i * b_stretch;
  a = a - i * a_stretch;
  vect2 u = b - a;
  vect2 v = flip ? rotate_90deg(i) : rotate_minus90deg(i);
  v = v * height;
  vect2 r = a - v;

  v = v * 2.0f; // Migrating to OpenGL, body parts need this for some reason

  glBindTexture(GL_TEXTURE_2D, tex);
  render_part(u, v, r);
}

static void render_rigid_part(GLuint tex, vect2 r, double radius, double rotation, bool flip) {
  float rad = flip ? -radius : radius;
  vect2 direction(cos(rotation) * rad, sin(rotation) * rad);
  render_body_part(tex, r - direction, r + direction, radius, 0.0, 0.0, flip);
}



void gl_render_kuski(float* frustrum, motorst* mot, valtozok* metadata, bool is_shadow) {

  if (TexSusp1 == 0) {
    TexHead = upload_affine_texture(Lgr->bike1.head);
    TexBody = upload_affine_texture(Lgr->bike1.body);
    TexUparm = upload_affine_texture(Lgr->bike1.up_arm);
    TexForarm = upload_affine_texture(Lgr->bike1.forarm);
    TexLeg = upload_affine_texture(Lgr->bike1.leg);
    TexThigh = upload_affine_texture(Lgr->bike1.thigh);
    TexBike = upload_affine_texture(Lgr->bike1.bike_part1);
    TexPart1 = upload_affine_texture(Lgr->bike1.bike_part1);
    TexPart2 = upload_affine_texture(Lgr->bike1.bike_part2);
    TexPart3 = upload_affine_texture(Lgr->bike1.bike_part3);
    TexPart4 = upload_affine_texture(Lgr->bike1.bike_part4);
    TexSusp1 = upload_affine_texture(Lgr->bike1.susp1);
    TexSusp2 = upload_affine_texture(Lgr->bike1.susp2);
    TexWheel = upload_affine_texture(Lgr->bike1.wheel);
  }


  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glUseProgram(KuskiShaderProgram);
  glUniform1i(glGetUniformLocation(KuskiShaderProgram, "IndexTexture"), 0);
  glUniform1i(glGetUniformLocation(KuskiShaderProgram, "PaletteTexture"), 1);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_1D, PaletteTexture);

  glBindVertexArray(KuskiVAO);
  glBindBuffer(GL_ARRAY_BUFFER, KuskiVBO);

  // set shadow color
  GLuint scloc = glGetUniformLocation(KuskiShaderProgram, "shadowColor");
  if (is_shadow) {
    glUniform4f(scloc, 0, 0, 0, .3);
  } else {
    glUniform4f(scloc, 0, 0, 0, 0);
  }


  // all subsequent tex will be texture0
  glActiveTexture(GL_TEXTURE0);

  double arm_position = metadata->ugrasnagysag;
  double turn_phase = metadata->baljobbv_f.forgas;

  // Check to see if bike is turning, and calculate the progress from -1.0 to 1.0 using cos
  bool is_turning = false;
  StretchEnabled = false;
  if (turn_phase < 0.999) {
      is_turning = true;
      turn_phase = -cos(turn_phase * PI);
  }


  // Calculate wheel position relative to screen
  vect2 left_wheel_r = mot->left_wheel.r;
  vect2 right_wheel_r = mot->right_wheel.r;


  // If turning, we will be rendering one wheel in the foreground
  // (usually they are rendered in background)
  bool left_wheel_in_back = true;
  bool right_wheel_in_back = true;
  if (is_turning) {
      if ((turn_phase > 0.0 && !mot->flipped_bike) || (turn_phase <= 0.0 && mot->flipped_bike)) {
          left_wheel_in_back = false;
      } else {
          right_wheel_in_back = false;
      }
  }

  // Render background wheels
  if (left_wheel_in_back) {
    render_rigid_part(TexWheel, left_wheel_r, WheelBackgroundRenderRadius, mot->left_wheel.rotation,
                     false);
  }
  if (right_wheel_in_back) {
    render_rigid_part(TexWheel, right_wheel_r, WheelBackgroundRenderRadius, mot->right_wheel.rotation,
                     false);
  }






  // Get the bike position and angle
  vect2 bike_r = mot->bike.r;
  vect2 bike_i = vect2(cos(mot->bike.rotation), sin(mot->bike.rotation));
  vect2 bike_j = rotate_90deg(bike_i);

  // If bike is turning, squish the bike
  if (is_turning) {
      StretchEnabled = true;
      StretchCenter = bike_r;
      bike_i.normalize();
      StretchAxis = bike_i;
      StretchFactor = turn_phase;
  }

  // If the bike is turned, flip the bike
  // Swap the wheels temporarily for the purposes of drawing the suspension
  if (mot->flipped_bike) {
      bike_i = Vect2null - bike_i;
      std::swap(left_wheel_r, right_wheel_r);
  }


  // Bike frame calculations. Rotate the bike frame by 0.62 radians
  BikeFrameX = 390.0;
  BikeFrameY = 420.0;
  constexpr double BIKE_FRAME_ROTATION = 0.62;
  constexpr double BIKE_FRAME_WIDTH = 0.0045;
  BikeFrameI = bike_i * (BIKE_FRAME_WIDTH * cos(BIKE_FRAME_ROTATION)) +
               bike_j * (BIKE_FRAME_WIDTH * sin(BIKE_FRAME_ROTATION));
  BikeFrameJ = rotate_90deg(BikeFrameI);
  if (mot->flipped_bike) {
      BikeFrameJ = Vect2null - BikeFrameJ;
  }
  BikeFrameR = bike_r;


  // Draw susp1
  vect2 susp1_r =
      BikeFrameI * (365.0 - BikeFrameX) + BikeFrameJ * (BikeFrameY - 292.0) + BikeFrameR;
  render_body_part(TexSusp1, left_wheel_r, susp1_r, 0.06, 0.05, 0.03, false);

  // Draw susp2
  vect2 susp2_r =
      BikeFrameI * (370.0 - BikeFrameX) + BikeFrameJ * (BikeFrameY - 520.0) + BikeFrameR;
  render_body_part(TexSusp2, susp2_r, right_wheel_r, 0.06, 0.0, 0.1, false);


  // Draw frame parts
  render_frame_part(TexPart1, &BikeBox1); // tank & bars
  render_frame_part(TexPart2, &BikeBox2); // motor
  render_frame_part(TexPart3, &BikeBox3); // 8
  render_frame_part(TexPart4, &BikeBox4); // mudguard



  // Calculations to draw the kuski
  vect2 body_r = mot->body_r;
  vect2 hip_r = body_r + BikeFrameI * 75.0 + BikeFrameJ * (-47.0);
  vect2 shoulder_r = body_r + BikeFrameI * 47.0 + BikeFrameJ * 65.0;
  vect2 neck_r = body_r + BikeFrameI * 41.0 + BikeFrameJ * 70.0;
  vect2 foot_r =
      BikeFrameI * (346.0 - BikeFrameX) + BikeFrameJ * (BikeFrameY - 514.0) + BikeFrameR;

  // Calculate how to bend the knee based on the hip and foot positions
  // (or how much the king has had to drink)
  vect2 knee_r;
  constexpr double THIGH_LENGTH = 0.51;
  constexpr double LEG_LENGTH = 0.51;
  if (mot->flipped_bike) {
      knee_r = circles_intersection(hip_r, foot_r, THIGH_LENGTH, LEG_LENGTH);
  } else {
      knee_r = circles_intersection(foot_r, hip_r, LEG_LENGTH, THIGH_LENGTH);
  }

  // Draw head
  float HeadRadius = 0.238;
  render_rigid_part(TexHead, mot->head_r, HeadRadius, mot->bike.rotation, mot->flipped_bike);

  // Hand is located on the handlebars, unless we are volting
  vect2 hand_r = susp1_r;
  if (arm_position > 0.0001) {
      // Invert the arm volt percentage to progress from 0->1
      arm_position = 1.0 - arm_position;
      // Left volt + facing left OR right volt + facing left -> Arm goes up
      bool arm_goes_up = true;
      if ((metadata->ugras1volt && !mot->flipped_bike) ||
          (!metadata->ugras1volt && mot->flipped_bike)) {
          // Right volt + facing left OR left volt + facing right -> arm goes down
          arm_goes_up = false;
      }

      // Describe the arm movement for up and down movements
      const double arm_apex_time = arm_goes_up ? 0.25 : 0.2;    // 0.0 to 1.0
      const double max_arm_rotation = arm_goes_up ? 2.7 : -1.6; // radians
      const double max_arm_stretch = arm_goes_up ? -0.3 : 0.15; // meters

      // Calculate arm movement progression away from neutral (0.0 to 1.0)
      double interpolation;
      if (arm_position < arm_apex_time) {
          interpolation = arm_position / arm_apex_time;
      } else {
          interpolation = 1.0 - (arm_position - arm_apex_time) / (1.0 - arm_apex_time);
      }

      // Calculate arm rotation and stretch
      double arm_rotation = max_arm_rotation * interpolation;
      double arm_stretch = max_arm_stretch * interpolation + 1.0;

      // Update hand position based on arm rotation and stretch
      vect2 arm_vector = hand_r - shoulder_r;
      if (!mot->flipped_bike) {
          arm_vector.rotate(-arm_rotation);
      } else {
          arm_vector.rotate(arm_rotation);
      }
      arm_vector = arm_vector * arm_stretch;
      hand_r = shoulder_r + arm_vector;
  }

  // Calculate how to bend the elbow based on shoulder and hand position
  constexpr double FORARM_LENGTH = 0.308 * 1.05;
  constexpr double UP_ARM_LENGTH = 0.328 * 1.05;
  vect2 elbow_r;
  if (mot->flipped_bike) {
      elbow_r = circles_intersection(hand_r, shoulder_r, FORARM_LENGTH, UP_ARM_LENGTH);
  } else {
      elbow_r = circles_intersection(shoulder_r, hand_r, UP_ARM_LENGTH, FORARM_LENGTH);
  }


  // Render body
  render_body_part(TexThigh, knee_r, hip_r, 0.14, 0.03, 0.1, mot->flipped_bike);
  render_body_part(TexLeg, foot_r, knee_r, 0.21, 0.03, 0.03, mot->flipped_bike);
  const GLuint body = TexBody; // shirt ? shirt : bike->body;
  render_body_part(body, hip_r, neck_r, 0.2, 0.1, 0.05, mot->flipped_bike);
  render_body_part(TexUparm, elbow_r, shoulder_r, 0.11, 0.08, 0.1, !mot->flipped_bike);
  render_body_part(TexForarm, hand_r, elbow_r, 0.076, 0.08, 0.1, mot->flipped_bike);



  // Render front wheels
  StretchEnabled = false;
  if (!left_wheel_in_back || !right_wheel_in_back) {
    glBindTexture(GL_TEXTURE_2D, TexWheel);
    if (mot->flipped_bike) {
      // If we had temporarily inverted the wheels earlier in this function, undo that now
      std::swap(left_wheel_r, right_wheel_r);
    }
    if (!left_wheel_in_back) {
      render_rigid_part(
        TexWheel, left_wheel_r, mot->left_wheel.radius, mot->left_wheel.rotation, false
      );
    }
    if (!right_wheel_in_back) {
      render_rigid_part(
        TexWheel, right_wheel_r, mot->right_wheel.radius, mot->right_wheel.rotation, false
      );
    }
  }

}
