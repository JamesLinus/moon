AC_DEFUN([MOONLIGHT_CHECK_MONO],
[
	MONO_REQUIRED_VERSION=2.0
	MONO_REQUIRED_BROWSER_VERSION=2.5

	MOON_ARG_ENABLED_BY_DEFAULT([browser-support], [Disable the browser plugin])
	browser_support=$enableval
	if test "x$browser_support" = xyes; then
		MONO_REQUIRED_VERSION=$MONO_REQUIRED_BROWSER_VERSION

		AC_ARG_WITH(mcspath, AC_HELP_STRING([--with-mcspath=<path>], []),
			[], [with_mcspath=../mcs])

		if test "x$with_mcspath" = "xno"; then
			AC_ERROR(You need to set the path to mcs)
		fi
			
		if test ! -d "$with_mcspath"; then
			AC_ERROR(mcs_path doesn't exist)
		fi

		MCS_PATH=$(cd "$with_mcspath" && pwd)
		AC_SUBST(MCS_PATH)

		AC_DEFINE([PLUGIN_SL_2_0], [1], [Enable Silverlight 2.0 support for the plugin])
	fi

	MOON_ARG_ENABLED_BY_DEFAULT([desktop-support], [Disable support for Moonlight-based desktop applications])
	desktop_support=$enableval
	if test "x$desktop_support" = xyes; then
		PKG_CHECK_MODULES(GTKSHARP, gtk-sharp-2.0)

		rsvg_sharp_pcs="rsvg-sharp-2.0 rsvg2-sharp-2.0"
		for pc in $rsvg_sharp_pcs; do
			PKG_CHECK_EXISTS($pc, [rsvg_sharp=$pc])
		done

		PKG_CHECK_MODULES(RSVGSHARP, $rsvg_sharp, [
			RSVG_SHARP=$rsvg_sharp
			AC_SUBST(RSVG_SHARP)
			AM_CONDITIONAL(HAVE_RSVG_SHARP, true)
		], [
			AM_CONDITIONAL(HAVE_RSVG_SHARP, false)
		])
	else
		AM_CONDITIONAL(HAVE_RSVG_SHARP, false)
	fi

	if test "x$desktop_support" = xno -a "x$browser_support" = xno; then
		AC_ERROR(You cannot disable both Browser and Desktop support)
	fi

 	PKG_CHECK_MODULES(MONO, mono >= $MONO_REQUIRED_VERSION)

	PKG_CHECK_MODULES(MONO_EXTENDED, mono >= $MONO_REQUIRED_BROWSER_VERSION, 
		mono_extended=yes, mono_extended=no)
	if test x$mono_extended = xyes; then
		dnl the logistics for these defines might change in the future
		dnl when 2.5+ becomes readily available and it turns out that 
		dnl we should always be disabling these features in the 
		dnl desktop-only scenario, but for now the 2.5 check is ok

		AC_DEFINE([MONO_ENABLE_APP_DOMAIN_CONTROL], [1], 
			[Whether Mono 2.5 is available and Deployment should create/destroy App Domains])

		AC_DEFINE([MONO_ENABLE_CORECLR_SECURITY], [1], 
			[Whether Mono 2.5 is available and CoreCLR security should be enabled])
	fi

	AC_DEFINE([SL_2_0], [1], [Enable Silverlight 2.0 support in the runtime])

	AM_CONDITIONAL(INCLUDE_MANAGED_CODE, true)
	AM_CONDITIONAL(INCLUDE_BROWSER_MANAGED_CODE, test x$browser_support = xyes)
	AM_CONDITIONAL(INCLUDE_DESKTOP_MANAGED_CODE, test x$desktop_support = xyes)

	SL_PROFILE=2.0
	AC_SUBST([SL_PROFILE])
])

