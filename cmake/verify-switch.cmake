execute_process(COMMAND "${NM}" -u "${ELF}" RESULT_VARIABLE status
                OUTPUT_VARIABLE symbols ERROR_VARIABLE diagnostic)
if(NOT status EQUAL 0 OR NOT symbols STREQUAL "")
  message(FATAL_ERROR "ELF symbol verification failed: ${diagnostic}${symbols}")
endif()
file(WRITE "${OUTPUT}/undefined.txt" "${symbols}")
file(SHA256 "${NRO}" digest)
file(WRITE "${OUTPUT}/build-manifest.json"
  "{\n  \"version\": \"${VERSION}\",\n  \"mesa_commit\": \"${MESA_COMMIT}\",\n  \"sha256\": \"${digest}\",\n  \"undefined_symbols\": 0\n}\n")
message(STATUS "biko3-runtime.nro SHA256 ${digest}")
