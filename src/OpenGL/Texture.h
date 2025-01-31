class Texture {
	public:
		Texture();
		~Texture();
		void Load(const char* filename);
		void Bind();
		void Unbind();
		unsigned int GetID() const { return m_id; }
		int GetWidth() const { return m_width; }
		int GetHeight() const { return m_height; }
	private:
		unsigned int m_id;
		int m_width, m_height, m_channels;
		unsigned char* m_data;
};
