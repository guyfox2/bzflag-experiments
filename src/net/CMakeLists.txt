add_library(bznet STATIC
    Address.cxx
    AresHandler.cxx
    multicast.cxx
    network.cxx
    Pack.cxx
    Ping.cxx
)

target_link_libraries(bznet
    ${c-ares_LIBRARY}
)
