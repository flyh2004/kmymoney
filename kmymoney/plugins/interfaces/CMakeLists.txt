set (libinterfaces_a_SOURCES
  kmmimportinterface.cpp kmmstatementinterface.cpp
  kmmviewinterface.cpp
  )

add_library(interfaces STATIC ${libinterfaces_a_SOURCES})
# TODO: fix dependencies
target_link_libraries(interfaces KF5::Service KF5::XmlGui Alkimia::alkimia Qt5::Sql Qt5::Xml)
