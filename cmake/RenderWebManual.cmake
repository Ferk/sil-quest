# SPDX-License-Identifier: EUPL-1.2
#
# Copyright (c) 2026 Fernando Carmona Varo
#
# Render the Sil-Quest AsciiDoc manual into the web bundle.  The generated HTML
# is not checked in; it is built from the source manual so the browser package
# stays in sync with the documentation.

if (NOT DEFINED ASCIIDOCTOR_EXECUTABLE)
    message(FATAL_ERROR "ASCIIDOCTOR_EXECUTABLE was not provided")
endif()

if (NOT DEFINED WEB_MANUAL_SOURCE)
    message(FATAL_ERROR "WEB_MANUAL_SOURCE was not provided")
endif()

if (NOT DEFINED WEB_MANUAL_OUTPUT)
    message(FATAL_ERROR "WEB_MANUAL_OUTPUT was not provided")
endif()

if (NOT DEFINED WEB_MANUAL_PDF_OUTPUT)
    message(FATAL_ERROR "WEB_MANUAL_PDF_OUTPUT was not provided")
endif()

if (NOT DEFINED WEB_MANUAL_IMAGE_SOURCE_DIR)
    message(FATAL_ERROR "WEB_MANUAL_IMAGE_SOURCE_DIR was not provided")
endif()

if (NOT DEFINED WEB_MANUAL_IMAGE_OUTPUT_DIR)
    message(FATAL_ERROR "WEB_MANUAL_IMAGE_OUTPUT_DIR was not provided")
endif()

if (NOT DEFINED WEB_MANUAL_STYLE_SOURCE_DIR)
    message(FATAL_ERROR "WEB_MANUAL_STYLE_SOURCE_DIR was not provided")
endif()

if (NOT DEFINED WEB_MANUAL_STYLE_OUTPUT_DIR)
    message(FATAL_ERROR "WEB_MANUAL_STYLE_OUTPUT_DIR was not provided")
endif()

if (NOT DEFINED WEB_MANUAL_FONT_SOURCE_DIR)
    message(FATAL_ERROR "WEB_MANUAL_FONT_SOURCE_DIR was not provided")
endif()

if (NOT DEFINED WEB_MANUAL_FONT_OUTPUT_DIR)
    message(FATAL_ERROR "WEB_MANUAL_FONT_OUTPUT_DIR was not provided")
endif()

if (NOT DEFINED WEB_MANUAL_PDF_TOC_NOTE_EXTENSION)
    message(FATAL_ERROR "WEB_MANUAL_PDF_TOC_NOTE_EXTENSION was not provided")
endif()

if (NOT DEFINED WEB_MANUAL_TOC_NOTE_SOURCE)
    message(FATAL_ERROR "WEB_MANUAL_TOC_NOTE_SOURCE was not provided")
endif()

file(REMOVE_RECURSE "${WEB_MANUAL_IMAGE_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${WEB_MANUAL_IMAGE_OUTPUT_DIR}")
file(COPY "${WEB_MANUAL_IMAGE_SOURCE_DIR}/"
    DESTINATION "${WEB_MANUAL_IMAGE_OUTPUT_DIR}")

file(REMOVE_RECURSE "${WEB_MANUAL_STYLE_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${WEB_MANUAL_STYLE_OUTPUT_DIR}")
file(COPY "${WEB_MANUAL_STYLE_SOURCE_DIR}/"
    DESTINATION "${WEB_MANUAL_STYLE_OUTPUT_DIR}")

file(REMOVE_RECURSE "${WEB_MANUAL_FONT_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${WEB_MANUAL_FONT_OUTPUT_DIR}")
file(COPY "${WEB_MANUAL_FONT_SOURCE_DIR}/"
    DESTINATION "${WEB_MANUAL_FONT_OUTPUT_DIR}")

execute_process(
    COMMAND "${ASCIIDOCTOR_EXECUTABLE}"
        -a "webfonts!"
        -a "nofooter"
        -a "toc=macro"
        -a "toc-title=Table of Contents"
        -a "toclevels=2"
        -a "stylesheet=manual.css"
        -a "stylesdir=styles"
        -o "${WEB_MANUAL_OUTPUT}"
        "${WEB_MANUAL_SOURCE}"
    RESULT_VARIABLE WEB_MANUAL_RENDER_RESULT
)

if (NOT WEB_MANUAL_RENDER_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to render ${WEB_MANUAL_SOURCE}")
endif()

if (DEFINED ASCIIDOCTOR_PDF_EXECUTABLE
    AND NOT "${ASCIIDOCTOR_PDF_EXECUTABLE}" STREQUAL ""
    AND NOT "${ASCIIDOCTOR_PDF_EXECUTABLE}" MATCHES "-NOTFOUND$")
    execute_process(
        COMMAND "${ASCIIDOCTOR_PDF_EXECUTABLE}"
            -r "${WEB_MANUAL_PDF_TOC_NOTE_EXTENSION}"
            -a "pdf-theme=manual-theme.yml"
            -a "pdf-themesdir=${WEB_MANUAL_STYLE_SOURCE_DIR}"
            -a "pdf-fontsdir=${WEB_MANUAL_FONT_OUTPUT_DIR}"
            -a "docimagesdir=${WEB_MANUAL_IMAGE_SOURCE_DIR}"
            -a "silquest-toc-note-file=${WEB_MANUAL_TOC_NOTE_SOURCE}"
            -o "${WEB_MANUAL_PDF_OUTPUT}"
            "${WEB_MANUAL_SOURCE}"
        RESULT_VARIABLE WEB_MANUAL_PDF_RENDER_RESULT
    )

    if (NOT WEB_MANUAL_PDF_RENDER_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to render PDF ${WEB_MANUAL_SOURCE}")
    endif()
endif()
