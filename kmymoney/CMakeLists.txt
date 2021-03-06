include(ECMAddAppIcon)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_BINARY_DIR}
                    ${CMAKE_CURRENT_BINARY_DIR}/dialogs/
                    ${CMAKE_CURRENT_SOURCE_DIR}/widgets/
                    ${CMAKE_CURRENT_BINARY_DIR}/widgets/
                    ${CMAKE_CURRENT_SOURCE_DIR}/mymoney/
                    ${CMAKE_CURRENT_SOURCE_DIR}/mymoney/storage/
                    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/
                    ${CMAKE_CURRENT_BINARY_DIR}/plugins/
                    ${CMAKE_CURRENT_SOURCE_DIR}/views/
                    ${CMAKE_CURRENT_SOURCE_DIR}/dialogs/
                    ${CMAKE_CURRENT_SOURCE_DIR}/converter/
                    ${CMAKE_CURRENT_BINARY_DIR}/dialogs/settings/
                    ${CMAKE_CURRENT_BINARY_DIR}/mymoney/storage/
                    ${CMAKE_CURRENT_BINARY_DIR}/mymoney/
                    ${CMAKE_CURRENT_SOURCE_DIR}/reports/
                    ${CMAKE_CURRENT_SOURCE_DIR}/wizards/endingbalancedlg/
                    ${CMAKE_CURRENT_BINARY_DIR}/wizards/endingbalancedlg/
                    ${CMAKE_CURRENT_SOURCE_DIR}/wizards/newinvestmentwizard/
                    ${CMAKE_CURRENT_BINARY_DIR}/wizards/newinvestmentwizard/
                    ${CMAKE_CURRENT_SOURCE_DIR}/wizards/newloanwizard/
                    ${CMAKE_CURRENT_BINARY_DIR}/wizards/newloanwizard/
                    ${CMAKE_CURRENT_SOURCE_DIR}/wizards/wizardpages/
                    ${CMAKE_CURRENT_SOURCE_DIR}/models/
                    ${CMAKE_CURRENT_BINARY_DIR}/models/
                    ${CMAKE_CURRENT_SOURCE_DIR}/icons/
                    ${CMAKE_CURRENT_BINARY_DIR}/icons/
                    ${CMAKE_CURRENT_BINARY_DIR}/payeeidentifier/ibanandbic/widgets/ # TODO: this line should be moved to the target it belongs
                    ${CMAKE_CURRENT_BINARY_DIR}/payeeidentifier/ibanandbic/
                    ${CMAKE_CURRENT_BINARY_DIR}/payeeidentifier/nationalaccount/
                    ${KMyMoney_SOURCE_DIR}/libkgpgfile/ )

add_subdirectory( mymoney )
add_subdirectory( models  )
add_subdirectory( plugins )
add_subdirectory( reports )
add_subdirectory( widgets )
add_subdirectory( dialogs )
add_subdirectory( views )
add_subdirectory( converter )
add_subdirectory( wizards )
add_subdirectory( pics )
add_subdirectory( html )
add_subdirectory( templates )
add_subdirectory( misc )
add_subdirectory( payeeidentifier )
add_subdirectory( icons )
if(BUILD_TESTING)
  add_subdirectory( tests )
endif()

set( _HEADERS kmymoneyutils.h kmymoneyglobalsettings.h )

########### settings code (kmm_config) STATIC ###############
set( kmm_config_SRCS kmymoneyglobalsettings.cpp )
kconfig_add_kcfg_files( kmm_config_SRCS kmymoneysettings.kcfgc )
add_library(kmm_config STATIC ${kmm_config_SRCS})
target_link_libraries(kmm_config KF5::WidgetsAddons KF5::ConfigWidgets Qt5::Sql Alkimia::alkimia)

########### common code (kmymoney_common) STATIC ###############
# will be linked into kmymoney, kmymoneytest, and libkmymoney.so

set( kmymoney_common_SRCS kmymoneyutils.cpp kstartuplogo.cpp kcreditswindow.cpp )
add_library(kmymoney_common STATIC ${kmymoney_common_SRCS})

target_link_libraries(kmymoney_common
  PUBLIC
    Qt5::Core
    KF5::ConfigGui
    KF5::WidgetsAddons
    Alkimia::alkimia
    kmm_widgets
    kmm_mymoney
    kmm_utils_webconnect
    kmm_utils_platformtools
)

# must build kmymoney/transactionsortoption.h
# from transactionsortoption.ui first
add_dependencies(kmymoney_common generate_base_ui_srcs kmm_config)
add_dependencies(wizardpages widgets)


if(USE_MODELTEST)
  set( kmymoney_common_LIBS ${kmymoney_common_LIBS} ${QT_QTTEST_LIBRARY})
endif(USE_MODELTEST)

########### kmymoney executable ###############

set( kmymoney_SRCS
  main.cpp
  kmymoney.cpp
  pluginloader.cpp
)

qt5_add_dbus_adaptor(kmymoney_SRCS org.kde.kmymoney.xml kmymoney.h KMyMoneyApp)
qt5_add_resources(kmymoney_SRCS kmymoney.qrc)

# collect application icons
file(GLOB_RECURSE KMYMONEY_APP_ICONS "${CMAKE_CURRENT_SOURCE_DIR}/icons/kmymoney/apps/*.png")
# add icons to application sources, to have them bundled
ecm_add_app_icon(kmymoney_SRCS ICONS ${KMYMONEY_APP_ICONS})

add_executable( kmymoney ${kmymoney_SRCS} )
target_link_libraries(kmymoney
  views
  reports
  kmymoney_base
  kmymoney_common
  newuserwizard
  newaccountwizard
  newinvestmentwizard
  newloanwizard
  endingbalancedlg
  wizardpages
  dialogs
  widgets
  settings
  converter
  models
  kmm_config
  kmm_widgets
  kmm_storage
  kmm_mymoney
  kgpgfile
  interfaces
  kmm_plugin
  Qt5::Core
  Qt5::Sql
  KF5::Archive
  KF5::ConfigGui
  KF5::WidgetsAddons
  KF5::CoreAddons
  KChart
  $<$<BOOL:${USE_MODELTEST}>:Qt5::Test>
  $<$<BOOL:${KF5Holidays_FOUND}>:KF5::Holidays>
)

# own plist magic for mac os
if(APPLE)
    # own plist template
    set_target_properties(kmymoney PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/MacOSXBundleInfo.plist.in)

    # the MacOSX bundle display name property (CFBundleDisplayName) is not currently supported by cmake,
    # so has to be set for all targets in this cmake file
    set(MACOSX_BUNDLE_DISPLAY_NAME KMyMoney)
    set_target_properties(kmymoney PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER "org.kde.KMyMoney")
    set_target_properties(kmymoney PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "KMyMoney")
    set_target_properties(kmymoney PROPERTIES MACOSX_BUNDLE_DISPLAY_NAME "KMyMoney")
    set_target_properties(kmymoney PROPERTIES MACOSX_BUNDLE_INFO_STRING "KMyMoney - Personal Finances Manager")
    set_target_properties(kmymoney PROPERTIES MACOSX_BUNDLE_LONG_VERSION_STRING "KMyMoney ${KDE_APPLICATIONS_VERSION}")
    set_target_properties(kmymoney PROPERTIES MACOSX_BUNDLE_SHORT_VERSION_STRING "${KDE_APPLICATIONS_VERSION_MAJOR}.${KDE_APPLICATIONS_VERSION_MINOR}")
    set_target_properties(kmymoney PROPERTIES MACOSX_BUNDLE_BUNDLE_VERSION "${KDE_APPLICATIONS_VERSION}")
    set_target_properties(kmymoney PROPERTIES MACOSX_BUNDLE_COPYRIGHT "2000-2016 The KMyMoney Authors")
endif()

########### install files ###############

install(TARGETS kmymoney ${INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES kmymoney.kcfg
  DESTINATION ${KCFG_INSTALL_DIR}
  )
install(FILES kmymoney.upd
  DESTINATION ${KCONF_UPDATE_INSTALL_DIR}
  )
install(FILES ${_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/kmymoney
  )
install(FILES org.kde.kmymoney.desktop
  DESTINATION ${XDG_APPS_INSTALL_DIR}
  )
install(FILES org.kde.kmymoney.appdata.xml
  DESTINATION ${KDE_INSTALL_METAINFODIR}
  )

install(FILES x-kmymoney.xml
  DESTINATION ${XDG_MIME_INSTALL_DIR})

install(FILES
   tips
   DESTINATION ${CMAKE_INSTALL_DATADIR}/kmymoney)

#UPDATE_XDG_MIMETYPES(${XDG_MIME_INSTALL_DIR})


