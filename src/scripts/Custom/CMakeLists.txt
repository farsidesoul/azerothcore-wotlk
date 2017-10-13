set(scripts_STAT_SRCS
  ${scripts_STAT_SRCS}
  ${AC_SCRIPTS_DIR}/Custom/custom_rates.cpp
)

AC_ADD_SCRIPT_LOADER("Custom" "ScriptLoader.h")

message(" -> Prepared: Custom")