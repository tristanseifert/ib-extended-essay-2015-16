/*
 * HDRRenderer.cpp
 *
 *  Created on: Aug 26, 2015
 *      Author: tristan
 */

#include "HDRRenderer.h"

#include <cassert>
#include <iostream>

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../../housekeeping/ServiceLocator.h"

// vertices for a full-screen quad
static const gl::GLfloat vertices[] = {
	-1.0f,  1.0f, 0.0f,		0.0f, 1.0f,
	-1.0f, -1.0f, 0.0f,		0.0f, 0.0f,
	 1.0f,  1.0f, 0.0f,		1.0f, 1.0f,
	 1.0f, -1.0f, 0.0f,		1.0f, 0.0f,
};

using namespace gl;
using namespace std;
namespace gfx {
/**
 * Sets up a basic framebuffer with a floating-point colour attachment.
 */
HDRRenderer::HDRRenderer() {
	// Load the shader program
	this->program = new ShaderProgram("rsrc/shader/hdr.shader");
    this->program->link();

    // set up the framebuffers
    setUpInputBuffers();

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
}

/**
 * Sets up the framebuffer into which the previous rendering stage will output.
 */
void HDRRenderer::setUpInputBuffers(void) {
    // allocate the FBO
    this->inFBO = new FrameBuffer();
    this->inFBO->bindRW();

    // get size of the viewport
	unsigned int width = ServiceLocator::window()->width;
	unsigned int height = ServiceLocator::window()->height;

    // colour (RGB) buffer (gets the full range of lighting values from scene)
    this->inColour = new Texture2D(1);
    this->inColour->allocateBlank(width, height, Texture2D::RGB16F);
    this->inColour->setDebugName("HDRColourIn");

	this->inFBO->attachTexture2D(this->inColour, FrameBuffer::ColourAttachment0);

    // Specify the buffers used for rendering
    FrameBuffer::AttachmentType buffers[] = {
    	FrameBuffer::ColourAttachment0,
		FrameBuffer::End
    };
    this->inFBO->setDrawBuffers(buffers);

    // Ensure completeness of the buffer.
    assert(FrameBuffer::isComplete() == true);
    FrameBuffer::unbindRW();

	// tell our program which texture units are used
    this->program->bind();
    this->program->setUniform1i("texInColour", this->inColour->unit);
}

HDRRenderer::~HDRRenderer() {
	delete this->program;
	delete this->inFBO;
	delete this->inColour;
	delete this->vao;
	delete this->vbo;
}

/**
 * Sets up for HDR rendering.
 */
void HDRRenderer::beforeRender(void) {
	// bind to the window framebuffer
    glDisable(GL_DEPTH_TEST);
}

/**
 * Extracts all the extra bright colours from the render buffer, and forwards
 * them to a different buffer.
 */
void HDRRenderer::render(void) {
	// use the "HDR" shader to get the bright areas to a separate buffer
	this->program->bind();
	this->inColour->bind();

	// render a full-screen quad
	vao->bind();
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

/**
 * Binds the HDR buffer.
 */
void HDRRenderer::bindHDRBuffer(void) {
	this->inFBO->bindRW();
}

/**
 * Sets the depth buffer that's attached to the FBO.
 */
void HDRRenderer::setDepthBuffer(Texture2D *depth, bool hasStencil) {
	// check if the texture changed
	if(this->inDepth == depth) { return; } else {
		if(this->inDepth != NULL) {
			this->inFBO->attachTexture2D(this->inDepth, FrameBuffer::DepthStencil);
		    assert(FrameBuffer::isComplete() == true);
			return;
		}
	}
	this->inDepth = depth;

	// attach the texture
    this->inFBO->bindRW();
    this->inFBO->attachTexture2D(this->inDepth, FrameBuffer::DepthStencil);

    assert(FrameBuffer::isComplete() == true);
    FrameBuffer::unbindRW();
}

/**
 * Sets the bloom renderer's framebuffer to use the colour texture.
 */
void HDRRenderer::setBloomRenderer(BloomRenderer *renderer) {
	renderer->setColourInputTex(this->inColour);
}
}