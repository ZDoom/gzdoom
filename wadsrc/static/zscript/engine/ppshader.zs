struct PPShader native
{
	native clearscope static void SetEnabled(string shaderName, bool enable);
	native clearscope static void SetUniform1f(string shaderName, string uniformName, float value);
	native clearscope static void SetUniform2f(string shaderName, string uniformName, vector2 value);
	native clearscope static void SetUniform3f(string shaderName, string uniformName, vector3 value);
	native clearscope static void SetUniform1i(string shaderName, string uniformName, int value);
}
