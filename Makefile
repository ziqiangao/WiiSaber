.SUFFIXES:

PROJECT_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

ifeq ($(strip $(DEVKITPPC)),)
$(error "DEVKITPPC is not set. Install devkitPro with devkitPPC and libogc, then source /etc/profile.d/devkit-env.sh")
endif

include $(DEVKITPPC)/wii_rules

TARGET      := wiisaber
BUILD       := build
WIRE_ROOT   := $(PROJECT_ROOT)/third_party/Wire3D
WIRE_MAKE   := $(WIRE_ROOT)/Wire/builds/gcc_wii
WIRE_LIBDIR := $(WIRE_ROOT)/Wire/lib/gcc_wii/Release
WIRE_LIB    := $(WIRE_LIBDIR)/libWire.a
IMPORTER    := $(WIRE_ROOT)/Tools/Importer/src
SOURCES     := src
DATA        :=
WIRE_INCLUDES := $(WIRE_ROOT)/Wire/Foundation \
               $(WIRE_ROOT)/Wire/Foundation/Wii \
               $(WIRE_ROOT)/Wire/Engine \
               $(WIRE_ROOT)/Wire/Renderer \
               $(WIRE_ROOT)/Wire/Renderer/GX \
               $(WIRE_ROOT)/Wire/InputSystem \
               $(WIRE_ROOT)/Wire/InputSystem/Wii \
               $(IMPORTER)

CFLAGS      = -g -O2 -Wall $(MACHDEP) $(INCLUDE) -DWIRE_WII -DNO_BULLET_PHYSICS_LIB -DNO_FREETYPE2_LIB
CXXFLAGS    = $(CFLAGS)
LDFLAGS     = -g $(MACHDEP)
LIBS        := -lWire -lwiiuse -lbte -lfat -logc -lm

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT   := $(PROJECT_ROOT)/$(TARGET)
export VPATH    := $(PROJECT_ROOT)/src \
                   $(PROJECT_ROOT)/src/app \
                   $(PROJECT_ROOT)/src/beatmap \
                   $(PROJECT_ROOT)/src/environment \
                   $(PROJECT_ROOT)/src/environment/demo \
                   $(PROJECT_ROOT)/src/environments/DefaultEnvironment/anicallbacks \
                   $(IMPORTER)
export DEPSDIR  := $(PROJECT_ROOT)/$(BUILD)

CPPFILES := main.cpp DefaultEnvironment.cpp DefaultEnvironmentLighting.cpp BeatmapLights.cpp BeatmapParserV3.cpp BeatmapParserV4.cpp scene.cpp Importer.cpp PicoPNG.cpp
CFILES   :=
SFILES   :=

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES   := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.S=.o)
export INCLUDE  := $(foreach dir,$(INCLUDES),-I$(PROJECT_ROOT)/$(dir)) \
                   $(foreach dir,$(WIRE_INCLUDES),-I$(dir)) \
                   -I$(PROJECT_ROOT)/src/environment \
                   -I$(PROJECT_ROOT)/src/environment/demo \
                   -I$(PROJECT_ROOT)/src/beatmap \
                   -I$(PROJECT_ROOT)/src/environments/DefaultEnvironment/anicallbacks \
                   -I$(PROJECT_ROOT)/$(BUILD) -I$(LIBOGC_INC)
export LIBPATHS := -L$(LIBOGC_LIB) -L$(WIRE_LIBDIR)

.PHONY: all clean wire $(BUILD)
all: $(BUILD)

wire: $(WIRE_LIB)

$(WIRE_LIB):
	$(MAKE) --no-print-directory -C $(WIRE_MAKE)

$(BUILD): $(WIRE_LIB)
	@mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(OUTPUT).dol: $(OUTPUT).elf

$(OUTPUT).elf: $(OFILES) $(WIRE_LIB)

clean:
	@rm -rf $(BUILD) $(OUTPUT).elf $(OUTPUT).dol

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).dol: $(OUTPUT).elf

$(OUTPUT).elf: $(OFILES) $(WIRE_LIB)

Importer.o: CXXFLAGS += -include $(PROJECT_ROOT)/src/wire_no_physics.h

-include $(DEPENDS)

endif
