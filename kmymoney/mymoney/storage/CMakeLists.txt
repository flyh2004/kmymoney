if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

set(storage_HEADERS
  imymoneystorage.h imymoneyserialize.h imymoneystorageformat.h
  kmymoneystorageplugin.h
  databasestoreableobject.h
  )

set(storage_a_SOURCES
  imymoneystorageformat.cpp mymoneystoragexml.cpp mymoneystoragedump.cpp
  mymoneyseqaccessmgr.cpp imymoneystorage.cpp imymoneyserialize.cpp
  mymoneystorageanon.cpp
  mymoneystoragesql.cpp
  mymoneydatabasemgr.cpp
  mymoneydbdef.cpp
  mymoneydbdriver.cpp
  kmymoneystorageplugin.cpp
  mymoneystoragenames.cpp
  )

# This library is actually not needed. It is built just for
# convenience, and then linked into kmm_mymoney.
add_library( kmm_storage STATIC ${storage_a_SOURCES} )
kde_target_enable_exceptions(kmm_storage PUBLIC)

target_link_libraries( kmm_storage
  PUBLIC
    KF5::Service
    KF5::I18n
    Qt5::Core
    Qt5::Xml
    Qt5::Sql
    Qt5::Gui
    Alkimia::alkimia
)

########### install files ###############

install(FILES ${storage_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/kmymoney COMPONENT Devel )

install(FILES kmymoney-sqlstorageplugin.desktop
  DESTINATION ${SERVICETYPES_INSTALL_DIR}
)
