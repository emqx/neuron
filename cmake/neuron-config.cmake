find_path(Neuron_INCLUDE_DIR neuron.h /usr/local/include/neuron)
find_library(Neuron_LIBRARY NAMES neuron-base PATHS /usr/local/lib)

if (Neuron_INCLUDE_DIR AND Neuron_LIBRARY)
    set(Neuron_FOUND TRUE)
endif()