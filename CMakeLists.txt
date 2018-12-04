cmake_minimum_required(
	VERSION
		3.8.0
)

project(DirectComposition
	LANGUAGES
		CXX
	VERSION
		1.0.0.0
)

add_executable(DirectComposition WIN32
	DirectComposition.cpp
	DirectComposition.exe.manifest
)

target_compile_definitions(DirectComposition
	PRIVATE
		UNICODE
		_UNICODE
)

target_link_libraries(DirectComposition
	PRIVATE
		d2d1
		d3d11
		dcomp
		dxgi
		dwmapi
		shcore
)

# bin
install(
	TARGETS
		DirectComposition
	DESTINATION
		bin
)
