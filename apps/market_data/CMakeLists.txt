add_executable(obz_market_data_publisher
    publisher.cpp
)

target_compile_features(obz_market_data_publisher
    PRIVATE
        cxx_std_20
)

target_link_libraries(obz_market_data_publisher
    PRIVATE
        obz_market::market_data
        obz_market::pipeline
)

add_executable(obz_market_data_receiver
    receiver.cpp
)

target_compile_features(obz_market_data_receiver
    PRIVATE
        cxx_std_20
)

target_link_libraries(obz_market_data_receiver
    PRIVATE
        obz_market::market_data
)
