#version 450
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aTint;
layout(push_constant) uniform Push { vec2 viewport; } pc;
layout(location=0) out vec2 vUV;
layout(location=1) out vec4 vTint;
void main(){
  vec2 ndc = vec2(aPos.x/(pc.viewport.x*0.5)-1.0, aPos.y/(pc.viewport.y*0.5)-1.0);
  gl_Position = vec4(ndc, 0.0, 1.0); vUV = aUV; vTint = aTint;
}
