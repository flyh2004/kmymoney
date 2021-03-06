
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kmymoney-sepaorders.json.in ${CMAKE_CURRENT_BINARY_DIR}/kmymoney-sepaorders.json @ONLY)

set( sepaOnlineTasks_SRCS
  tasks/sepaonlinetransferimpl.cpp
  ui/sepacredittransferedit.cpp
  sepastorageplugin.cpp
  sepaonlinetasksloader.cpp
)

set( sepaOnlineTasks_HEADER
  tasks/sepaonlinetransfer.h
)

ki18n_wrap_ui( sepaOnlineTasks_SRCS
  ui/sepacredittransferedit.ui
)

add_library(konlinetasks_sepa_interface INTERFACE)

add_library(konlinetasks_sepa MODULE
  ${sepaOnlineTasks_SRCS}
)

# TODO some dependencies can be moved to konlinetasks_sepa
target_link_libraries( konlinetasks_sepa_interface
  INTERFACE
  kmm_mymoney
  kmm_widgets
  kmm_plugin
  onlinetask_interfaces
  Qt5::Xml
  Qt5::Core
  Qt5::Gui
  Qt5::Sql
)

target_link_libraries(konlinetasks_sepa
  PUBLIC
    konlinetasks_sepa_interface
  PRIVATE
    KF5::I18n
    KF5::Service
    kmm_config
    kmm_utils_validators
    payeeidentifier_iban_bic_widgets
    KF5::ItemModels
)

install(TARGETS konlinetasks_sepa
        DESTINATION "${KDE_INSTALL_PLUGINDIR}/kmymoney/")

install(FILES ${sepaOnlineTasks_HEADER}
        DESTINATION "${INCLUDE_INSTALL_DIR}/kmymoney/onlinetasks/sepa/"
        COMPONENT Devel)
