package com.tharunbirla.librecuts.shaders

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class RetroArchSlangPresetParserTest {
    @Test fun parsesOrderedShaderPassesAndTextures() {
        val preset = RetroArchSlangPresetParser.parsePreset(
            """
            preset_name = "CRT Test"
            shader1 = "shaders/first.slang"
            shader0 = "shaders/base.slang"
            textures = "mask;noise"
            """.trimIndent()
        )
        assertEquals("CRT Test", preset.title)
        assertEquals(listOf("shaders/base.slang", "shaders/first.slang"), preset.shaderPaths)
        assertEquals(listOf("mask", "noise"), preset.textures)
    }

    @Test fun parsesSlangParameterPragmas() {
        val parameters = RetroArchSlangPresetParser.parseParameters(
            "#pragma parameter CRT " + "\"CRT Strength\" 0.5 0.0 1.0 0.01"
        )
        assertEquals(1, parameters.size)
        assertEquals("CRT", parameters[0].name)
        assertEquals("CRT Strength", parameters[0].description)
        assertEquals("0.5", parameters[0].defaultValue)
    }

    @Test fun rejectsTraversalPaths() {
        assertFalse(RetroArchSlangPresetParser.validateRelativeAssetPath("../../outside.slang"))
        assertFalse(RetroArchSlangPresetParser.validateRelativeAssetPath("/absolute.slang"))
        assertTrue(RetroArchSlangPresetParser.validateRelativeAssetPath("shaders/crt.slang"))
    }
}
