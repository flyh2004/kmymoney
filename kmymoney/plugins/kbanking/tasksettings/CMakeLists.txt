set ( kbanking_tasksettings_SRCS
  credittransfersettingsbase.cpp
)

add_library( kmm_kbanking_tasksettings STATIC ${kbanking_tasksettings_SRCS} )

target_link_libraries( kmm_kbanking_tasksettings
  kmm_utils_validators
  Qt5::Gui
  Qt5::Xml
  Qt5::Sql
  KF5::I18n
  konlinetasks_sepa_interface
)
