#include <string>
#include <vector>
#include <memory>
#include <map>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

class GLFontManager
{
public:
	struct AtlasGroup
	{
		// Grid atlas contains an array of square grids with side length
		// gridMaxSize. Each grid takes a single glyph and splits it into
		// cells that inform the fragment shader which curves of the glyph
		// intersect that cell. The cell contains coords to data in the bezier
		// atlas. The bezier atlas contains the actual bezier curves for each
		// glyph. All data for a single glyph must lie in a single row, although
		// multiple glyphs can be in one row. Each bezier curve takes three
		// "RGBA pixels" (12 bytes) of data.
		// Both atlases also encode some extra information, which is explained
		// where it is used in the code.
		GLuint gridAtlasId;
		uint8_t *gridAtlas;
		uint16_t nextGridPos[2]; // XY pixel coordinates
		bool full; // For faster checking
		bool uploaded;

		GLuint glyphDataBufId, glyphDataBufTexId;
		uint8_t *glyphDataBuf;
		uint16_t glyphDataBufOffset; // pixel coordinates
	};

	struct Glyph
	{
		uint16_t size[2]; // Width and height in FT units
		int16_t offset[2]; // Offset of glyph in FT units
		uint16_t bezierAtlasPos[2]; // XZ pixel coordinates (Z being atlas index)
		int16_t advance; // Amount to advance after character in FT units
	};

public: // TODO: private
	std::vector<AtlasGroup> atlases;
	std::map<FT_Face, std::map<uint32_t, Glyph> > glyphs;
	FT_Library ft;
	FT_Face defaultFace;
	GLuint glyphShader, uGridAtlas, uTransform;
	GLuint uGlyphData;

	GLFontManager();

	AtlasGroup * GetOpenAtlasGroup();

public:
	~GLFontManager();

	static std::shared_ptr<GLFontManager> singleton;
	static std::shared_ptr<GLFontManager> GetFontManager();

	FT_Face GetFontFromPath(std::string fontPath);
	FT_Face GetFontFromName(std::string fontName);
	FT_Face GetDefaultFont();

	Glyph * GetGlyphForCodepoint(FT_Face face, uint32_t point);
	void LoadASCII(FT_Face face);
	void UploadAtlases();

	void UseGlyphShader();
	void SetShaderTransform(glm::mat4 transform);
	void UseAtlasTextures(uint16_t atlasIndex);
};

class GLLabel
{
public:
	enum class Align
	{
		Start,
		Center,
		End
	};

	struct Color
	{
		uint8_t r,g,b,a;
	};

private:
	struct GlyphVertex
	{
		// XY coords of the vertex
		glm::vec2 pos;

		// Bit 0 (low) is norm coord X (varies per vertex)
		// Bit 1 is norm coord Y (varies per vertex)
		// Bits 2-31 are texel offset (byte offset / 4) into
		//   glyphDataBuf (same for all verticies of a glyph)
		uint32_t data;

		// RGBA color [0,255]
		Color color;
	};

	// Each of these arrays store the same "set" of data, but different versions
	// of it. Consequently, each of these will be exactly the same length
	// (except verts, which is six times longer than the other two, since
	// six verts per glyph).
	// Can't put them all into one array, because verts is needed alone as a
	// buffer to upload to the GPU, and text is needed alone mostly for GetText.
	std::u32string text;
	std::vector<GlyphVertex> verts;
	std::vector<GLFontManager::Glyph *> glyphs;

	std::shared_ptr<GLFontManager> manager;
	GLuint vertBuffer, caretBuffer;
	bool showingCaret;
	size_t caretPosition;
	float prevTime, caretTime;

public:
	GLLabel();
	~GLLabel();

	void InsertText(std::u32string text, size_t index, glm::vec4 color, FT_Face face);
	void RemoveText(size_t index, size_t length);
	inline void SetText(std::u32string text, glm::vec4 color, FT_Face face) {
		this->RemoveText(0, this->text.size());
		this->InsertText(text, 0, color, face);
	}
	inline void AppendText(std::u32string text, glm::vec4 color, FT_Face face) {
		this->InsertText(text, this->text.size(), color, face);
	}

	inline std::u32string GetText() { return this->text; }

	void SetHorzAlignment(Align horzAlign);
	void SetVertAlignment(Align vertAlign);
	void ShowCaret(bool show) { showingCaret = show; }
	void SetCaretPosition(int position) { caretTime = 0; caretPosition = glm::clamp(position, 0, (int)text.size()); }
	int GetCaretPosition() { return caretPosition; }

	// Render the label. Also uploads modified textures as necessary. 'time'
	// should be passed in monotonic seconds (no specific zero time necessary).
	void Render(float time, glm::mat4 transform);
};
