/*
 * SceneLighting.cpp
 *
 *  Created on: Aug 22, 2015
 *      Author: tristan
 */

#include "SceneLighting.h"
#include "HDRRenderer.h"
#include "FXAARenderer.h"

#include <cassert>
#include <iostream>

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../level/primitives/lights/DirectionalLight.h"
#include "../level/primitives/lights/PointLight.h"
#include "../level/primitives/lights/SpotLight.h"

#include "../../housekeeping/ServiceLocator.h"

// vertices for a full-screen quad
static const gl::GLfloat vertices[] = {
	-1.0f,  1.0f, 0.0f,		0.0f, 1.0f,
	-1.0f, -1.0f, 0.0f,		0.0f, 0.0f,
	 1.0f,  1.0f, 0.0f,		1.0f, 1.0f,
	 1.0f, -1.0f, 0.0f,		1.0f, 0.0f,
};


static const glm::vec3 cubeLightColours[] = {
	glm::vec3(1.0f, 0.0f, 0.0f),
	glm::vec3(0.0f, 1.0f, 0.0f),
	glm::vec3(0.0f, 0.0f, 1.0f),
	glm::vec3(1.0f, 0.5f, 0.0f) * 10.f,
};

static const glm::vec3 cubeLightPositions[] = {
	glm::vec3( 1.5f,  2.0f, -2.5f),
	glm::vec3( 1.5f,  0.2f, -1.5f),
	glm::vec3(-1.3f,  1.0f, -1.5f),
	glm::vec3( 1.5f,  2.0f, -1.5f)
};

#import "skyboxVertices.h"

using namespace gl;
using namespace std;
namespace gfx {

/**
 * Allocates the various textures needed for the G-Buffer.
 */
SceneLighting::SceneLighting() {
	// Load the shader program
	this->program = new ShaderProgram("rsrc/shader/lighting.shader");
    this->program->link();

    // allocate the FBO
    this->fbo = new FrameBuffer();
    this->fbo->bindRW();

    // get size of the viewport
	unsigned int width = ServiceLocator::window()->width;
	unsigned int height = ServiceLocator::window()->height;

    // Normal colour (RGB) and shininess(A) buffer
    this->gNormal = new Texture2D(0);
    this->gNormal->allocateBlank(width, height, Texture2D::RGBA16F);
    this->gNormal->setDebugName("gBufNormal");

	this->fbo->attachTexture2D(this->gNormal, FrameBuffer::ColourAttachment0);

    // Colour and specular buffer
    this->gAlbedoSpec = new Texture2D(1);
    this->gAlbedoSpec->allocateBlank(width, height, Texture2D::RGBA8);
    this->gAlbedoSpec->setUsesLinearFiltering(true);
    this->gAlbedoSpec->setDebugName("gBufAlbedoSpec");

	this->fbo->attachTexture2D(this->gAlbedoSpec, FrameBuffer::ColourAttachment1);

    // Depth and stencil
    this->gDepth = new Texture2D(2);
    this->gDepth->allocateBlank(width, height, Texture2D::Depth24Stencil8);
    this->gDepth->setDebugName("gBufDepth");

    this->fbo->attachTexture2D(this->gDepth, FrameBuffer::DepthStencil);

    // Specify the buffers used for rendering (sans depth)
    FrameBuffer::AttachmentType buffers[] = {
    	FrameBuffer::ColourAttachment0,
		FrameBuffer::ColourAttachment1,
		FrameBuffer::End
    };
    this->fbo->setDrawBuffers(buffers);

    // Ensure completeness of the buffer.
    assert(FrameBuffer::isComplete() == true);
    FrameBuffer::unbindRW();

    // set up a VAO and VBO for the full-screen quad
	vao = new VertexArray();
	vbo = new Buffer(Buffer::Array, Buffer::StaticDraw);

	vao->bind();
	vbo->bind();

	vbo->bufferData(sizeof(vertices), (void *) vertices);

	vao->registerVertexAttribPointer(0, 3, VertexArray::Float,
									 5 * sizeof(GLfloat), 0);
	vao->registerVertexAttribPointer(1, 2, VertexArray::Float,
									 5 * sizeof(GLfloat), 3 * sizeof(GLfloat));

	VertexArray::unbind();

	// tell our program which texture units are used
    this->program->bind();
    this->program->setUniform1i("gNormal", this->gNormal->unit);
    this->program->setUniform1i("gAlbedoSpec", this->gAlbedoSpec->unit);
    this->program->setUniform1i("gDepth", this->gDepth->unit);

	// Compile skybox shader and set up some vertex data
    this->skyboxProgram = new ShaderProgram("rsrc/shader/skybox.shader");
	this->skyboxProgram->link();

	vaoSkybox = new VertexArray();
	vboSkybox = new Buffer(Buffer::Array);

	vaoSkybox->bind();
	vboSkybox->bind();

	vboSkybox->bufferData(sizeof(skyboxVertices), (void *) skyboxVertices);

	vaoSkybox->registerVertexAttribPointer(0, 3, VertexArray::Float,
									 	   3 * sizeof(GLfloat), 0);
	VertexArray::unbind();

	// load cubemap texture
	this->skyboxTexture = new TextureCube(0);
	this->skyboxTexture->setDebugName("SkyCube");
	this->skyboxTexture->loadFromImages("rsrc/tex/cube/", true);
	TextureCube::unbind();

	// set up test lights
	this->setUpTestLights();
}

/**
 * Sets up the default lights for testing.
 */
void SceneLighting::setUpTestLights(void) {
	// set up a test directional light
	DirectionalLight *dir = new DirectionalLight();
	dir->setDirection(glm::vec3(-0.2f, -1.0f, -0.3f));
	dir->setColour(glm::vec3(0.85, 0.85, 0.75));

	this->addLight(dir);

	// set up a test spot light
	this->spot = new SpotLight();
	this->spot->setInnerCutOff(12.5f);
	this->spot->setOuterCutOff(17.5f);
	this->spot->setLinearAttenuation(0.1f);
	this->spot->setQuadraticAttenuation(0.8f);
	this->spot->setColour(glm::vec3(1.0f, 0.33f, 0.33f));

	this->addLight(this->spot);

	// point lights
	for(int i = 0; i < 4; i++) {
		PointLight *light = new PointLight();

		light->setPosition(glm::vec3(cubeLightPositions[i]));
		light->setColour(glm::vec3(cubeLightColours[i]));

		light->setLinearAttenuation(0.7f);
		light->setQuadraticAttenuation(1.8f);

		this->addLight(light);
	}
}

SceneLighting::~SceneLighting() {
	delete this->program;
	delete this->fbo;
	delete this->gAlbedoSpec;
	delete this->gDepth;
	delete this->gNormal;
	delete this->vao;
	delete this->vbo;

	// delete the lights
	for(auto &light : this->lights) {
		delete light;
	}
}

/**
 * Clear the output buffer
 */
void SceneLighting::beforeRender(void) {
    // clear the output buffer
    glClear(GL_COLOR_BUFFER_BIT);

    // ensure we do not write to the depth buffer during lighting
    glDepthMask(GL_FALSE);
}

/**
 * Renders the lighting pass.
 */
void SceneLighting::render(void) {
	// use our lighting shader, bind textures and set their locations
	this->program->bind();

	this->gNormal->bind();
	this->gAlbedoSpec->bind();
	this->gDepth->bind();

	if(this->shadowTexture) {
		this->shadowTexture->bind();
	}

	// Send ambient light
    this->program->setUniform1f("ambientLight.Intensity", 0.05f);
	this->program->setUniformVec("ambientLight.Colour", glm::vec3(1.0, 1.0, 1.0));

	// send the different types of light
	this->spot->setDirection(this->viewDirection);
	this->spot->setPosition(this->viewPosition);

	this->sendLightsToShader();

	// send the camera position and inverse view matrix
	this->program->setUniformVec("viewPos", this->viewPosition);

	// Inverse projection and view matrix
	glm::mat4 viewMatrixInv = glm::inverse(this->viewMatrix);
	this->program->setUniformMatrix("viewMatrixInv", viewMatrixInv);

	glm::mat4 projMatrixInv = glm::inverse(this->projectionMatrix);
	this->program->setUniformMatrix("projMatrixInv", projMatrixInv);

	// render a full-screen quad
	vao->bind();
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	VertexArray::unbind();

	// unbind textures
	this->gNormal->unbind();
	this->gAlbedoSpec->unbind();
	this->gDepth->unbind();

	// render the skybox
	renderSkybox();
}

/**
 * Sends the different lights' data to the shader, which is currently bound.
 */
void SceneLighting::sendLightsToShader(void) {
	// set up counters
	int numDirectional, numPoint, numSpot;
	numDirectional = numPoint = numSpot = 0;

	// go through each type of light
	for(auto &light : this->lights) {
		switch(light->getType()) {
			case lights::AbstractLight::Directional:
				light->sendToProgram(numDirectional++, this->program);
				break;

			case lights::AbstractLight::Point:
				light->sendToProgram(numPoint++, this->program);
				break;

			case lights::AbstractLight::Spot:
				 light->sendToProgram(numSpot++, this->program);
				break;

			default:
				cerr << "Unknown light type: " << light->getType() << endl;
				break;
		}
	}

    // send how many of each type of light (directional, point, spot) we have
    glm::vec3 lightNums = glm::vec3(numDirectional, numPoint, numSpot);
    this->program->setUniformVec("LightCount", lightNums);
}

/**
 * Renders the skybox.
 */
void SceneLighting::renderSkybox(void) {
	// set up some state for the skybox
	glDepthFunc(GL_LEQUAL);

	this->skyboxProgram->bind();

	// calculate a new view matrix with translation components removed
	glm::mat4 newView = glm::mat4(glm::mat3(this->viewMatrix));

	this->skyboxProgram->setUniformMatrix("view", newView);
	this->skyboxProgram->setUniformMatrix("projection", this->projectionMatrix);

	// bind VAO, texture, then draw
	this->vaoSkybox->bind();

	this->skyboxTexture->bind();
	this->skyboxProgram->setUniform1i("skyboxTex", this->skyboxTexture->unit);

	glDrawArrays(GL_TRIANGLES, 0, 36);
}

/**
 * Unbinds any information and prepares the next frame.
 */
void SceneLighting::afterRender(void) {
    // allow successive render passes to render depth
    glDepthMask(GL_TRUE);
}

/**
 * Binds the various G-buffer elements before the scene itself is rendered. This
 * sets up three textures, into which the following data is rendered:
 *
 * 1. Positions (RGB)
 * 2. Colour (RGB) plus specular (A)
 * 3. Normal vectors (RGB)
 *
 * Following a call to this function, the scene should be rendered, and when
 * this technique is rendered, it will render the final geometry with lighting
 * applied.
 */
void SceneLighting::bindGBuffer(void) {
	this->fbo->bindRW();

	// re-attach the depth texture
    this->fbo->attachTexture2D(this->gDepth, FrameBuffer::DepthStencil);
    assert(FrameBuffer::isComplete() == true);
}

/**
 * Sets the HDR renderer's framebuffer to use our depth stencil.
 */
void SceneLighting::setHDRRenderer(HDRRenderer *renderer) {
	renderer->setDepthBuffer(this->gDepth, true);
}

/**
 * Sets the FXAA renderer to re-use our albedo texture.
 */
void SceneLighting::setFXAARenderer(FXAARenderer *renderer) {
	renderer->setColourInputTex(this->gAlbedoSpec);
}

/**
 * Adds a light to the list of lights. Each frame, these lights are sent to the
 * GPU.
 *
 * @note We assume ownership of the objects once they are added, so they are
 * deleted when this class goes away.
 */
void SceneLighting::addLight(lights::AbstractLight *light) {
	this->lights.push_back(light);
}

/**
 * Removes a previously added light.
 *
 * @return 0 if the light was removed, -1 otherwise.
 */
int SceneLighting::removeLight(lights::AbstractLight *light) {
	auto position = std::find(this->lights.begin(),
							  this->lights.end(), light);

	if(position > this->lights.end()) {
		return -1;
	} else {
		this->lights.erase(position);
		return 0;
	}
}

/**
 * Sets the texture in which the shadow data is stored, as well as the light
 * space matrix.
 */
void SceneLighting::setShadowTexture(Texture2D *tex, glm::mat4 lightSpaceMtx) {
	// set texture, if it changed
	if(this->shadowTexture != tex) {
		this->shadowTexture = tex;
		this->shadowLightSpaceTransform = lightSpaceMtx;
	}

	// bind program and send texture param
	this->program->bind();
    this->program->setUniform1i("gShadowMap", this->shadowTexture->unit);

    // send the light space matrix's inverse
    glm::mat4 matrix = glm::inverse(lightSpaceMtx);
    this->program->setUniformMatrix("lightToViewMtx", matrix);
}
}