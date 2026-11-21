# FR-046 / research R-02: high diagnostic quality for every PixelCSS target.
# Documented warning suppressions (see docs/build/warnings.md) go into
# PXC_WARNING_SUPPRESSIONS as /wdNNNN entries with a justification there.

set(PXC_WARNING_SUPPRESSIONS
    # (none)
)

function(pxc_set_warnings target)
    target_compile_options(${target} PRIVATE
        /W4 /WX /permissive- /utf-8
        /Zc:__cplusplus /Zc:preprocessor
        /EHsc /sdl /guard:cf
        /external:anglebrackets /external:W0
        ${PXC_WARNING_SUPPRESSIONS}
        $<$<CONFIG:Release>:/O2 /GL>
    )
    target_compile_definitions(${target} PRIVATE
        UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    )
    get_target_property(_type ${target} TYPE)
    if(_type STREQUAL "EXECUTABLE")
        target_link_options(${target} PRIVATE /guard:cf $<$<CONFIG:Release>:/LTCG>)
    elseif(_type STREQUAL "STATIC_LIBRARY")
        set_target_properties(${target} PROPERTIES STATIC_LIBRARY_OPTIONS "$<$<CONFIG:Release>:/LTCG>")
    endif()
endfunction()
