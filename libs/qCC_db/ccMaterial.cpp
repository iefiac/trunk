//##########################################################################
//#                                                                        #
//#                              CLOUDCOMPARE                              #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 or later of the License.      #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#          COPYRIGHT: EDF R&D / TELECOM ParisTech (ENST-TSI)             #
//#                                                                        #
//##########################################################################

//Always first
#include "ccIncludeGL.h"

//Local
#include "ccMaterial.h"
#include "ccBasicTypes.h"
#include "ccLog.h"

//Qt
#include <QMap>
#include <QUuid>
#include <QFileInfo>
#include <QDataStream>
#include <QOpenGLTexture>

//System
#include <assert.h>

//Textures DB
QMap<QString, QImage> s_textureDB;
QMap<QString, QSharedPointer<QOpenGLTexture>> s_openGLTextureDB;

ccMaterial::ccMaterial(QString name)
	: m_name(name)
	, m_uniqueID(QUuid::createUuid().toString())
	, m_diffuseFront(ccColor::bright)
	, m_diffuseBack(ccColor::bright)
	, m_ambient(ccColor::night)
	, m_specular(ccColor::night)
	, m_emission(ccColor::night)
{
	setShininess(50.0);
};

ccMaterial::ccMaterial(const ccMaterial& mtl)
	: m_name(mtl.m_name)
	, m_textureFilename(mtl.m_textureFilename)
	, m_uniqueID(mtl.m_uniqueID)
	, m_diffuseFront(mtl.m_diffuseFront)
	, m_diffuseBack(mtl.m_diffuseBack)
	, m_ambient(mtl.m_ambient)
	, m_specular(mtl.m_specular)
	, m_emission(mtl.m_emission)
	, m_shininessFront(mtl.m_shininessFront)
	, m_shininessBack(mtl.m_shininessFront)
{
}

void ccMaterial::setDiffuse(const ccColor::Rgbaf& color)
{
	setDiffuseFront(color);
	setDiffuseBack(color);
}

void ccMaterial::setShininess(float val)
{
	setShininessFront(val);
	setShininessBack(0.8f * val);
}

void ccMaterial::setTransparency(float val)
{
	m_diffuseFront.a = val;
	m_diffuseBack.a  = val;
	m_ambient.a      = val;
	m_specular.a     = val;
	m_emission.a     = val;
}

void ccMaterial::applyGL(const QOpenGLContext* context, bool lightEnabled, bool skipDiffuse) const
{
	//get the set of OpenGL functions (version 2.1)
	QOpenGLFunctions_2_1* glFunc = context->versionFunctions<QOpenGLFunctions_2_1>();
	assert(glFunc != nullptr);

	if (glFunc == nullptr)
		return;

	if (lightEnabled)
	{
		if (!skipDiffuse)
		{
			glFunc->glMaterialfv(GL_FRONT, GL_DIFFUSE, m_diffuseFront.rgba);
			glFunc->glMaterialfv(GL_BACK,  GL_DIFFUSE, m_diffuseBack.rgba);
		}
		glFunc->glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT,   m_ambient.rgba);
		glFunc->glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR,  m_specular.rgba);
		glFunc->glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION,  m_emission.rgba);
		glFunc->glMaterialf (GL_FRONT,          GL_SHININESS, m_shininessFront);
		glFunc->glMaterialf (GL_BACK,           GL_SHININESS, m_shininessBack);
	}
	else
	{
		glFunc->glColor4fv(m_diffuseFront.rgba);
	}
}

bool ccMaterial::loadAndSetTexture(QString absoluteFilename)
{
	if (absoluteFilename.isEmpty())
	{
		ccLog::Warning(QString("[ccMaterial::loadAndSetTexture] filename can't be empty!"));
		return false;
	}
	ccLog::PrintDebug(QString("[ccMaterial::loadAndSetTexture] absolute filename = %1").arg(absoluteFilename));

	if (s_textureDB.contains(absoluteFilename))
	{
		//if the image is already in memory, we simply update the texture filename for this amterial
		m_textureFilename = absoluteFilename;
	}
	else
	{
		//otherwise, we try to load the corresponding file
		QImage image(absoluteFilename);
		if (image.isNull())
		{
			ccLog::Warning(QString("[ccMaterial::loadAndSetTexture] Failed to load image '%1'").arg(absoluteFilename));
			return false;
		}
		else
		{
			setTexture(image, absoluteFilename, true);
		}
	}

	return true;
}

void ccMaterial::setTexture(QImage image, QString absoluteFilename/*=QString()*/, bool mirrorImage/*=true*/)
{
	ccLog::PrintDebug(QString("[ccMaterial::setTexture] absoluteFilename = '%1' / size = %2 x %3").arg(absoluteFilename).arg(image.width()).arg(image.height()));

	if (absoluteFilename.isEmpty())
	{
		//if the user hasn't provided any filename, we generate a fake one
		absoluteFilename = QString("tex_%1.jpg").arg(m_uniqueID);
		assert(!s_textureDB.contains(absoluteFilename));
	}
	else
	{
		//if the texture has already been loaded
		if (s_textureDB.contains(absoluteFilename))
		{
			//check that the size is compatible at least
			if (s_textureDB[absoluteFilename].size() != image.size())
			{
				ccLog::Warning(QString("[ccMaterial] A texture with the same name (%1) but with a different size has already been loaded!").arg(absoluteFilename));
			}
			m_textureFilename = absoluteFilename;
			return;
		}
	}

	m_textureFilename = absoluteFilename;

	//insert image into DB if necessary
	s_textureDB[m_textureFilename] = mirrorImage ? image.mirrored() : image;
}

const QImage ccMaterial::getTexture() const
{
	return s_textureDB[m_textureFilename];
}

GLuint ccMaterial::getTextureID() const
{
	if (QOpenGLContext::currentContext())
	{
		const QImage image = getTexture();
		if (image.isNull())
		{
			return 0;
		}
		QSharedPointer<QOpenGLTexture> tex = s_openGLTextureDB[m_textureFilename];
		if (!tex)
		{
			tex = QSharedPointer<QOpenGLTexture>::create(QOpenGLTexture::Target2D);
			tex->setAutoMipMapGenerationEnabled(false);
			tex->setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Linear);
			tex->setFormat(QOpenGLTexture::RGB8_UNorm);
			tex->setData(getTexture(), QOpenGLTexture::DontGenerateMipMaps);
			tex->create();
			s_openGLTextureDB[m_textureFilename] = tex;
		}
		return tex->textureId();
	}
	else
	{
		return 0;
	}

}

bool ccMaterial::hasTexture() const
{
	return m_textureFilename.isEmpty() ? false : !s_textureDB[m_textureFilename].isNull();
}

void ccMaterial::MakeLightsNeutral(const QOpenGLContext* context)
{
	//get the set of OpenGL functions (version 2.1)
	QOpenGLFunctions_2_1* glFunc = context->versionFunctions<QOpenGLFunctions_2_1>();
	assert(glFunc != nullptr);

	if (glFunc == nullptr)
		return;

	GLint maxLightCount;
	glFunc->glGetIntegerv(GL_MAX_LIGHTS, &maxLightCount);
	
	for (int i=0; i<maxLightCount; ++i)
	{
		if (glFunc->glIsEnabled(GL_LIGHT0 + i))
		{
			float diffuse[4];
			float ambiant[4];
			float specular[4];

			glFunc->glGetLightfv(GL_LIGHT0 + i, GL_DIFFUSE, diffuse);
			glFunc->glGetLightfv(GL_LIGHT0 + i, GL_AMBIENT, ambiant);
			glFunc->glGetLightfv(GL_LIGHT0 + i, GL_SPECULAR, specular);

			 diffuse[0] =  diffuse[1] =  diffuse[2] = ( diffuse[0] +  diffuse[1] +  diffuse[2]) / 3;	//'mean' (gray) value
			 ambiant[0] =  ambiant[1] =  ambiant[2] = ( ambiant[0] +  ambiant[1] +  ambiant[2]) / 3;	//'mean' (gray) value
			specular[0] = specular[1] = specular[2] = (specular[0] + specular[1] + specular[2]) / 3;	//'mean' (gray) value

			glFunc->glLightfv(GL_LIGHT0 + i, GL_DIFFUSE, diffuse);
			glFunc->glLightfv(GL_LIGHT0 + i, GL_AMBIENT, ambiant);
			glFunc->glLightfv(GL_LIGHT0 + i, GL_SPECULAR, specular);
		}
	}
}

QImage ccMaterial::GetTexture(QString absoluteFilename)
{
	return s_textureDB[absoluteFilename];
}

void ccMaterial::AddTexture(QImage image, QString absoluteFilename)
{
	s_textureDB[absoluteFilename] = image;
}

void ccMaterial::ReleaseTextures()
{
	if (!QOpenGLContext::currentContext())
	{
		ccLog::Warning("[ccMaterial::ReleaseTextures] No valid OpenGL context");
		return;
	}

	s_openGLTextureDB.clear();
}

void ccMaterial::releaseTexture()
{
	if (m_textureFilename.isEmpty())
	{
		//nothing to do
		return;
	}

	assert(QOpenGLContext::currentContext());

	s_textureDB.remove(m_textureFilename);
	s_openGLTextureDB.remove(m_textureFilename);
	m_textureFilename.clear();
}

bool ccMaterial::toFile(QFile& out) const
{
	QDataStream outStream(&out);

	//material name (dataVersion >= 20)
	outStream << m_name;
	//texture (dataVersion >= 20)
	outStream << m_textureFilename;
	//material colors (dataVersion >= 20)
	//we don't use QByteArray here as it has its own versions!
	if (out.write((const char*)m_diffuseFront.rgba, sizeof(float) * 4) < 0)
		return WriteError();
	if (out.write((const char*)m_diffuseBack.rgba, sizeof(float) * 4) < 0)
		return WriteError();
	if (out.write((const char*)m_ambient.rgba, sizeof(float) * 4) < 0)
		return WriteError();
	if (out.write((const char*)m_specular.rgba, sizeof(float) * 4) < 0)
		return WriteError();
	if (out.write((const char*)m_emission.rgba, sizeof(float) * 4) < 0)
		return WriteError();
	//material shininess (dataVersion >= 20)
	outStream << m_shininessFront;
	outStream << m_shininessBack;

	return true;
}

bool ccMaterial::fromFile(QFile& in, short dataVersion, int flags)
{
	QDataStream inStream(&in);

	//material name (dataVersion>=20)
	inStream >> m_name;
	if (dataVersion < 37)
	{
		//texture (dataVersion>=20)
		QImage texture;
		inStream >> texture;
		setTexture(texture,QString(),false);
	}
	else
	{
		//texture 'filename' (dataVersion>=37)
		inStream >> m_textureFilename;
	}
	//material colors (dataVersion>=20)
	if (in.read((char*)m_diffuseFront.rgba,sizeof(float)*4) < 0) 
		return ReadError();
	if (in.read((char*)m_diffuseBack.rgba,sizeof(float)*4) < 0) 
		return ReadError();
	if (in.read((char*)m_ambient.rgba,sizeof(float)*4) < 0) 
		return ReadError();
	if (in.read((char*)m_specular.rgba,sizeof(float)*4) < 0) 
		return ReadError();
	if (in.read((char*)m_emission.rgba,sizeof(float)*4) < 0) 
		return ReadError();
	//material shininess (dataVersion>=20)
	inStream >> m_shininessFront;
	inStream >> m_shininessBack;

	return true;
}

bool ccMaterial::compare(const ccMaterial& mtl) const
{
	if (	mtl.m_name != m_name
		||	mtl.m_textureFilename != m_textureFilename
		||	mtl.m_shininessFront != m_shininessFront
		||	mtl.m_shininessBack != m_shininessBack
		||	mtl.m_ambient != m_ambient
		||	mtl.m_specular != m_specular
		||	mtl.m_emission != m_emission
		||	mtl.m_diffuseBack != m_diffuseBack
		||	mtl.m_diffuseFront != m_diffuseFront
		||	mtl.m_diffuseFront != m_diffuseFront)
	{
		return false;
	}

	return true;
}
