from conan import ConanFile
from conan.tools.cmake import cmake_layout

class CalToolRecipe(ConanFile):
    name = "caltool"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"
    build_policy = "missing"

    requires = (
        "opencv/4.13.0",
        "protobuf/6.33.5",
        "nlohmann_json/3.11.3",
        "spdlog/1.17.0",
        "asio/1.38.0",
    )

    default_options = {
        # OpenCV: reduce image/video codec graph
        "opencv/*:with_tiff": False,
        "opencv/*:with_gdal": False,
        "opencv/*:with_openexr": False,
        "opencv/*:with_jpeg2000": False,
        "opencv/*:with_protobuf": False,
        "opencv/*:with_ffmpeg": False,

        # Useful extras to avoid pulling more transitive deps
        "opencv/*:with_webp": False,
        "opencv/*:with_avif": False,
        "opencv/*:with_gtk": False,
        "opencv/*:with_qt": False,
    }

    def configure(self):
        if self.settings.os == "Linux":
            # Only if this option exists in your OpenCV recipe.
            # If it does not exist, Conan will error.
            self.options["opencv/*"].with_wayland = False

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.28]")
        # TODO: Strangely removing this fails to build proto files normally. Commenting out incase this a me only problem.
        # self.tool_requires("protobuf/6.33.5")

    def test_requirements(self):
        self.test_requires("gtest/1.17.0")

    def layout(self):
        cmake_layout(self)
