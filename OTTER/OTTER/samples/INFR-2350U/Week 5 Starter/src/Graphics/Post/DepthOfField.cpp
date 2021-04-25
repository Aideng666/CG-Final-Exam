#include "DepthOfField.h"

void DepthOfField::Init(unsigned width, unsigned height)
{
    //Depth of Scene
    int index = int(_buffers.size());
    _buffers.push_back(new Framebuffer());
    _buffers[index]->AddColorTarget(GL_RGBA8);
    _buffers[index]->AddDepthTarget();
    _buffers[index]->Init(width, height);

    //Horizontal Blur
    index++;
    _buffers.push_back(new Framebuffer());
    _buffers[index]->AddColorTarget(GL_RGBA8);
    _buffers[index]->AddDepthTarget();
    _buffers[index]->Init(unsigned(width / _downscale), unsigned(height / _downscale));

    //Blur Vertical
    index++;
    _buffers.push_back(new Framebuffer());
    _buffers[index]->AddColorTarget(GL_RGBA8);
    _buffers[index]->AddDepthTarget();
    _buffers[index]->Init(unsigned(width / _downscale), unsigned(height / _downscale));

    //Composite
    index++;
    _buffers.push_back(new Framebuffer());
    _buffers[index]->AddColorTarget(GL_RGBA8);
    _buffers[index]->AddDepthTarget();
    _buffers[index]->Init(width, height);

    //Composite2
    index++;
    _buffers.push_back(new Framebuffer());
    _buffers[index]->AddColorTarget(GL_RGBA8);
    _buffers[index]->AddDepthTarget();
    _buffers[index]->Init(width, height);

    //Color of Scene
    index++;
    _buffers.push_back(new Framebuffer());
    _buffers[index]->AddColorTarget(GL_RGBA8);
    _buffers[index]->AddDepthTarget();
    _buffers[index]->Init(width, height);

    //Loads the shaders
    index = int(_shaders.size());
    _shaders.push_back(Shader::Create());
    _shaders[index]->LoadShaderPartFromFile("shaders/passthrough_vert.glsl", GL_VERTEX_SHADER);
    _shaders[index]->LoadShaderPartFromFile("shaders/passthrough_frag.glsl", GL_FRAGMENT_SHADER);
    _shaders[index]->Link();

    index++;
    _shaders.push_back(Shader::Create());
    _shaders[index]->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
    _shaders[index]->LoadShaderPartFromFile("shaders/Post/DepthOfField/dof_depth_frag.glsl", GL_FRAGMENT_SHADER);
    _shaders[index]->Link();

    index++;
    _shaders.push_back(Shader::Create());
    _shaders[index]->LoadShaderPartFromFile("shaders/passthrough_vert.glsl", GL_VERTEX_SHADER);
    _shaders[index]->LoadShaderPartFromFile("shaders/Post/DepthOfField/dof_horizontal_blur.glsl", GL_FRAGMENT_SHADER);
    _shaders[index]->Link();

    index++;
    _shaders.push_back(Shader::Create());
    _shaders[index]->LoadShaderPartFromFile("shaders/passthrough_vert.glsl", GL_VERTEX_SHADER);
    _shaders[index]->LoadShaderPartFromFile("shaders/Post/DepthOfField/dof_blur_vertical.glsl", GL_FRAGMENT_SHADER);
    _shaders[index]->Link();

    index++;
    _shaders.push_back(Shader::Create());
    _shaders[index]->LoadShaderPartFromFile("shaders/passthrough_vert.glsl", GL_VERTEX_SHADER);
    _shaders[index]->LoadShaderPartFromFile("shaders/Post/DepthOfField/dof_composite_frag.glsl", GL_FRAGMENT_SHADER);
    _shaders[index]->Link();

    index++;
    _shaders.push_back(Shader::Create());
    _shaders[index]->LoadShaderPartFromFile("shaders/passthrough_vert.glsl", GL_VERTEX_SHADER);
    _shaders[index]->LoadShaderPartFromFile("shaders/Post/DepthOfField/dof_composite2_frag.glsl", GL_FRAGMENT_SHADER);
    _shaders[index]->Link();

    _pixelSize = glm::vec2(1.f / width, 1.f / height);

    PostEffect::Init(width, height);
}

void DepthOfField::ApplyEffect(PostEffect* buffer)
{
    //Color
    BindShader(0);

    buffer->BindColorAsTexture(0, 0, 0);
    buffer->BindDepthAsTexture(0, 1);

    _buffers[5]->RenderToFSQ();

    buffer->UnbindTexture(0);

    UnbindShader();

    //Depth
    BindShader(1);

    buffer->BindColorAsTexture(0, 0, 0);

    _buffers[4]->RenderToFSQ();

    buffer->UnbindTexture(0);

    UnbindShader();

    //Composite 2
    BindShader(5);

    BindColorAsTexture(5, 0, 0);
    BindColorAsTexture(0, 0, 1);

    _buffers[1]->RenderToFSQ();

    UnbindTexture(1);
    UnbindTexture(0);

    UnbindShader();

    //Gets the Blur
    for (unsigned i = 0; i < _passes; i++)
    {
        //Horizontal
        BindShader(2);
        _shaders[2]->SetUniform("u_PixelSize", _pixelSize.x);
        _shaders[2]->SetUniform("u_FocalLength", _focalLength);
        _shaders[2]->SetUniform("u_FocalDistance", _planeInFocus);

        BindColorAsTexture(1, 0, 0);

        _buffers[2]->RenderToFSQ();

        UnbindTexture(0);

        UnbindShader();

        //Vertical
        BindShader(3);
        _shaders[3]->SetUniform("u_PixelSize", _pixelSize.y);
        _shaders[3]->SetUniform("u_FocalLength", _focalLength);
        _shaders[3]->SetUniform("u_FocalDistance", _planeInFocus);

        BindColorAsTexture(2, 0, 0);

        _buffers[1]->RenderToFSQ();

        UnbindTexture(0);

        UnbindShader();
    }

    //Composite
    BindShader(4);

    BindColorAsTexture(5, 0, 0);
    BindColorAsTexture(1, 0, 1);

    _buffers[0]->RenderToFSQ();

    UnbindTexture(1);
    UnbindTexture(0);

    UnbindShader();
}

void DepthOfField::Reshape(unsigned width, unsigned height)
{
    _buffers[0]->Reshape(width, height);
    _buffers[1]->Reshape(unsigned(width / _downscale), unsigned(height / _downscale));
    _buffers[2]->Reshape(unsigned(width / _downscale), unsigned(height / _downscale));
    _buffers[3]->Reshape(width, height);
    _buffers[4]->Reshape(width, height);
    _buffers[5]->Reshape(width, height);
}

float DepthOfField::GetRadius() const
{
    return _radius;
}

float DepthOfField::GetDownscale() const
{
    return _downscale;
}

unsigned DepthOfField::GetPasses() const
{
    return _passes;
}

float DepthOfField::GetZNear() const
{
    return _zNear;
}

float DepthOfField::GetZFar() const
{
    return _zFar;
}

float DepthOfField::GetAperture() const
{
    return _aperture;
}

float DepthOfField::GetPlaneInFocus() const
{
    return _planeInFocus;
}

float DepthOfField::GetFocalLength() const
{
    return _focalLength;
}

float DepthOfField::GetAmount()
{
    return _amount;
}

std::vector<Shader::sptr> DepthOfField::GetShaders() const
{
    return _shaders;
}

void DepthOfField::SetRadius(float radius)
{
    _radius = radius;
}

void DepthOfField::SetDownscale(float downscale)
{
    _downscale = downscale;
    Reshape(_buffers[0]->_width, _buffers[0]->_height);
}

void DepthOfField::SetPasses(unsigned passes)
{
    _passes = passes;
}

void DepthOfField::SetZNear(float dist)
{
    _zNear = dist;
}

void DepthOfField::SetZFar(float dist)
{
    _zFar = dist;
}

void DepthOfField::SetAperture(float aperture)
{
    _aperture = aperture;
}

void DepthOfField::SetPlaneInFocus(float plane)
{
    _planeInFocus = plane;
}

void DepthOfField::SetFocalLength(float length)
{
    _focalLength = length;
}

void DepthOfField::SetAmount(float amount)
{
    _amount = amount;
}

