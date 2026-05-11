

#include <node.h>
#include <napi.h>
#include <vector>

#include "./elma/lib.h"
#include "elma/simulation.h"


namespace demo {


static Napi::FunctionReference constructor;
double DT = FRAME_FPS200;


class ElmaSim : public Napi::ObjectWrap<ElmaSim> {
public:
  Simulation *sim;
  //static Napi::FunctionReference constructor;

  static Napi::Object NewInstance(Napi::Env env, Simulation* sim) {

    auto js_object = constructor.New({});
    ElmaSim::Unwrap(js_object)->sim = sim;
    return js_object;
  }

  static Napi::Object Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "ElmaSim", {
      InstanceMethod("step", &ElmaSim::Step),
      InstanceMethod("copy", &ElmaSim::Copy),
      InstanceMethod("dead", &ElmaSim::Dead),
      InstanceMethod("finished", &ElmaSim::Finished),
      InstanceMethod("body_r", &ElmaSim::BodyR),
    });

    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();
    return exports;
  }

  ElmaSim(const Napi::CallbackInfo& info) : Napi::ObjectWrap<ElmaSim>(info) {}

private:

  void Finalize(Napi::BasicEnv env) {
    delete sim;
  }

  void Step(const Napi::CallbackInfo& info) {
    if (info.Length() != 1) {
      Napi::TypeError::New(info.Env(), "Wrong number of arguments").ThrowAsJavaScriptException();
    }
    if (!info[0].IsNumber()) {
      Napi::TypeError::New(info.Env(), "Arg not number").ThrowAsJavaScriptException();
    }
    Napi::Number keys = info[0].As<Napi::Number>();
    gameloop_step(sim, DT, keys.Int32Value(), {});
  }

  Napi::Value Copy(const Napi::CallbackInfo& info) {
    auto js_object = constructor.New({});
    ElmaSim::Unwrap(js_object)->sim = new Simulation(*sim);
    return js_object;
  }
  Napi::Value Dead(const Napi::CallbackInfo& info) {
    return Napi::Boolean::New(info.Env(), sim->meghalt);
  }
  Napi::Value Finished(const Napi::CallbackInfo& info) {
    return Napi::Number::New(info.Env(), sim->megvanido);
  }
  Napi::Value BodyR(const Napi::CallbackInfo& info) {
    auto obj = Napi::Object::New(info.Env());
    obj.Set("x", sim->motor.body_r.x);
    obj.Set("y", sim->motor.body_r.y);
    return obj;
  }
};






void ElmaInit(const Napi::CallbackInfo& info) {
  elma_init();
}

static Napi::Value InitGame(const Napi::CallbackInfo& info) {

  Simulation *sim = nullptr;

  if (info.Length() != 1) {
    Napi::TypeError::New(info.Env(), "Wrong number of arguments").ThrowAsJavaScriptException();
  }
  if (info[0].IsNumber()) {
    auto internalIdx = info[0].As<Napi::Number>();
    sim = init_game(internalIdx.Int32Value());
  } else if (info[0].IsString()) {
    auto lev = info[0].As<Napi::String>();
    sim = init_game(lev.Utf8Value().c_str());
  } else {
    Napi::TypeError::New(info.Env(), "Expected number or string level name").ThrowAsJavaScriptException();
  }

  return ElmaSim::NewInstance(info.Env(), sim);
}


static Napi::Value GetKeyboardBuffer(const Napi::CallbackInfo& info) {

  size_t keyBufSize;
  const unsigned char* buf = get_keyboard_buffer(&keyBufSize);

  Napi::Env env = info.Env();
  return Napi::ArrayBuffer::New(
      env,
      (void*)buf,
      keyBufSize,
      [](Napi::Env /*env*/, void* finalizeData) {}
  );
}

void HandlEvents(const Napi::CallbackInfo& info) {
  handle_events();
}

static Napi::Value KeyIsDown(const Napi::CallbackInfo& info) {
  if (info.Length() != 1) {
    Napi::TypeError::New(info.Env(), "Wrong number of arguments").ThrowAsJavaScriptException();
  }
  if (!info[0].IsNumber()) {
    Napi::TypeError::New(info.Env(), "Arg not number").ThrowAsJavaScriptException();
  }
  Napi::Number code = info[0].As<Napi::Number>();
  bool r = key_is_down(code.Int32Value());
  return Napi::Boolean::New(info.Env(), r);
}

static Napi::Value KeyJustPressed(const Napi::CallbackInfo& info) {
  if (info.Length() != 1) {
    Napi::TypeError::New(info.Env(), "Wrong number of arguments").ThrowAsJavaScriptException();
  }
  if (!info[0].IsNumber()) {
    Napi::TypeError::New(info.Env(), "Arg not number").ThrowAsJavaScriptException();
  }
  Napi::Number code = info[0].As<Napi::Number>();
  bool r = key_just_pressed(code.Int32Value());
  return Napi::Boolean::New(info.Env(), r);
}

void Noop(const Napi::CallbackInfo& info) {
}

void AdjustZoom(const Napi::CallbackInfo& info) {
  if (info.Length() != 1) {
    Napi::TypeError::New(info.Env(), "Wrong number of arguments").ThrowAsJavaScriptException();
  }
  if (!info[0].IsNumber()) {
    Napi::TypeError::New(info.Env(), "Arg not number").ThrowAsJavaScriptException();
  }
  Napi::Number adj = info[0].As<Napi::Number>();
  adjust_zoom(adj.DoubleValue());
}

void Sleep(const Napi::CallbackInfo& info) {
  if (info.Length() != 1) {
    Napi::TypeError::New(info.Env(), "Wrong number of arguments").ThrowAsJavaScriptException();
  }
  if (!info[0].IsNumber()) {
    Napi::TypeError::New(info.Env(), "Arg not number").ThrowAsJavaScriptException();
  }
  Napi::Number adj = info[0].As<Napi::Number>();
  sleep(adj.DoubleValue());
}
void SetDT(const Napi::CallbackInfo& info) {
  if (info.Length() != 1) {
    Napi::TypeError::New(info.Env(), "Wrong number of arguments").ThrowAsJavaScriptException();
  }
  if (!info[0].IsNumber()) {
    Napi::TypeError::New(info.Env(), "Arg not number").ThrowAsJavaScriptException();
  }
  Napi::Number adj = info[0].As<Napi::Number>();
  DT = adj.DoubleValue();
}
void SetQuality(const Napi::CallbackInfo& info) {
  if (info.Length() != 1) {
    Napi::TypeError::New(info.Env(), "Wrong number of arguments").ThrowAsJavaScriptException();
  }
  if (!info[0].IsNumber()) {
    Napi::TypeError::New(info.Env(), "Arg not number").ThrowAsJavaScriptException();
  }
  Napi::Number adj = info[0].As<Napi::Number>();
  set_quality(adj.Int32Value());
}

std::vector<Simulation*> _GetShadows(Napi::Array arr) {
  std::vector<Simulation*> shadows;
  for (int i=0; i<arr.Length(); i++) {
    auto val = arr.Get(i);
    if (val.IsObject()) {
      auto sim = ElmaSim::Unwrap(val.As<Napi::Object>());
      shadows.push_back(sim->sim);
    }
  }
  return shadows;
}

void Step(const Napi::CallbackInfo& info) {

  if (info.Length() != 2 && info.Length() != 3) {
    Napi::TypeError::New(info.Env(), "Wrong number of arguments").ThrowAsJavaScriptException();
  }

  if (!info[0].IsObject()) {
    Napi::TypeError::New(info.Env(), "Arg 1 (Sim) not object").ThrowAsJavaScriptException();
  }
  Napi::Number keys = info[1].As<Napi::Number>();

  if (!info[1].IsNumber()) {
    Napi::TypeError::New(info.Env(), "Arg 2 (key mask) not number").ThrowAsJavaScriptException();
  }
  auto sim = ElmaSim::Unwrap(info[0].ToObject());


  std::vector<Simulation*> shadows;
  if (info.Length() == 3) {
    if (!info[2].IsArray()) {
      Napi::TypeError::New(info.Env(), "Arg 3 (shadows) not array").ThrowAsJavaScriptException();
    }
    shadows = _GetShadows(info[2].As<Napi::Array>());
  }

  gameloop_step(
    sim->sim,
    DT,
    keys.Int32Value(),
    shadows
  );
}

void Render(const Napi::CallbackInfo& info) {
  if (info.Length() != 1 && info.Length() != 2) {
    Napi::TypeError::New(info.Env(), "Wrong number of arguments").ThrowAsJavaScriptException();
  }
  if (!info[0].IsObject()) {
    Napi::TypeError::New(info.Env(), "Arg not object").ThrowAsJavaScriptException();
  }

  std::vector<Simulation*> shadows;
  if (info.Length() == 2) {
    if (!info[1].IsArray()) {
      Napi::TypeError::New(info.Env(), "Arg 2 (shadows) not array").ThrowAsJavaScriptException();
    }
    shadows = _GetShadows(info[1].As<Napi::Array>());
  }

  Napi::Object adj = info[0].As<Napi::Object>();
  ElmaSim* elmaSim = Napi::ObjectWrap<ElmaSim>::Unwrap(adj);
  gameloop_render(elmaSim->sim, shadows);
}




static Napi::FunctionReference cb;
void render_callback() { cb({}); }
void SetGLRenderCallback(const Napi::CallbackInfo& info) {
  cb = Napi::Persistent(info[0].As<Napi::Function>());
  set_gl_render_callback(&render_callback);
}

static Napi::Number get_number(const Napi::CallbackInfo& info, size_t i) {
  if (!info[i].IsNumber()) {
    Napi::TypeError::New(info.Env(), "Arg not number").ThrowAsJavaScriptException();
  }
  return info[i].As<Napi::Number>();
}

void GLRenderBox(const Napi::CallbackInfo& info) {
  if (info.Length() != 10) {
    Napi::TypeError::New(info.Env(), "Wrong number of arguments").ThrowAsJavaScriptException();
  }
  float r = get_number(info, 0).FloatValue();
  float g = get_number(info, 1).FloatValue();
  float b = get_number(info, 2).FloatValue();
  float a = get_number(info, 3).FloatValue();
  int x = get_number(info, 4).Int32Value();
  int y = get_number(info, 5).Int32Value();
  int rows = get_number(info, 6).Int32Value();
  int cols = get_number(info, 7).Int32Value();
  int paddingX = get_number(info, 8).Int32Value();
  int paddingY = get_number(info, 9).Int32Value();
  gl_render_box(r, g, b, a, x, y, rows, cols, paddingX, paddingY);
}
void GLRenderText(const Napi::CallbackInfo& info) {
  if (info.Length() != 9) {
    Napi::TypeError::New(info.Env(), "Wrong number of arguments").ThrowAsJavaScriptException();
  }
  float r = get_number(info, 0).FloatValue();
  float g = get_number(info, 1).FloatValue();
  float b = get_number(info, 2).FloatValue();
  float a = get_number(info, 3).FloatValue();
  int x = get_number(info, 4).Int32Value();
  int y = get_number(info, 5).Int32Value();
  int colOff = get_number(info, 6).Int32Value();
  int rowOff = get_number(info, 7).Int32Value();
  if (!info[8].IsString()) {
    Napi::TypeError::New(info.Env(), "Arg not string").ThrowAsJavaScriptException();
  }
  Napi::String text = info[8].As<Napi::String>();
  gl_render_text(r, g, b, a, x, y, colOff, rowOff, text.Utf8Value().c_str());
}

static Napi::Object Init(Napi::Env env, Napi::Object exports) {

  ElmaSim::Init(env, exports);

  exports["elmaInit"] = Napi::Function::New(env, ElmaInit);
  exports["initGame"] = Napi::Function::New(env, InitGame);
  exports["handleEvents"] = Napi::Function::New(env, HandlEvents);
  exports["keyIsDown"] = Napi::Function::New(env, KeyIsDown);
  exports["keyJustPressed"] = Napi::Function::New(env, KeyJustPressed);
  exports["noop"] = Napi::Function::New(env, Noop);
  exports["getKeyboardBuffer"] = Napi::Function::New(env, GetKeyboardBuffer);
  exports["adjustZoom"] = Napi::Function::New(env, AdjustZoom);
  exports["sleep"] = Napi::Function::New(env, Sleep);
  exports["setDT"] = Napi::Function::New(env, SetDT);
  exports["step"] = Napi::Function::New(env, Step);
  exports["render"] = Napi::Function::New(env, Render);
  exports["setQuality"] = Napi::Function::New(env, SetQuality);
  exports["setGLRenderCallback"] = Napi::Function::New(env, SetGLRenderCallback);
  exports["renderBox"] = Napi::Function::New(env, GLRenderBox);
  exports["renderText"] = Napi::Function::New(env, GLRenderText);

  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)

}
