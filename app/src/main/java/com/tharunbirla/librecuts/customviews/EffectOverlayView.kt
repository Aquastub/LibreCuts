package com.tharunbirla.librecuts.customviews

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View
import kotlin.math.max
import kotlin.math.min

/** Selection frame for a localized effect. The normalized region is saved in the project model. */
class EffectOverlayView @JvmOverloads constructor(
    context: Context, attrs: AttributeSet? = null, defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    var centerXFraction: Float = 0.5f
        private set
    var centerYFraction: Float = 0.5f
        private set
    var widthFraction: Float = 0.35f
        private set
    var heightFraction: Float = 0.35f
        private set

    var isEditingMode: Boolean = false
        set(value) {
            field = value
            visibility = if (value) VISIBLE else GONE
            invalidate()
        }

    var onBoundsChanged: ((Float, Float, Float, Float) -> Unit)? = null

    private val borderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(0, 229, 255)
        style = Paint.Style.STROKE
        strokeWidth = 3.5f * resources.displayMetrics.density
    }
    private val fillPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = 0x1833B5E5
        style = Paint.Style.FILL
    }
    private val handlePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        style = Paint.Style.FILL
    }
    private val labelPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textSize = 11f * resources.displayMetrics.scaledDensity
        typeface = android.graphics.Typeface.create("sans-serif-medium", android.graphics.Typeface.BOLD)
    }
    private val bounds = RectF()
    private val handleRadius = 9f * resources.displayMetrics.density
    private var dragging = false
    private var resizing = false
    private var resizeCorner = 0 // TL, TR, BL, BR
    private var lastX = 0f
    private var lastY = 0f

    fun setRegion(x: Float, y: Float, w: Float, h: Float) {
        widthFraction = w.coerceIn(0.05f, 1f)
        heightFraction = h.coerceIn(0.05f, 1f)
        centerXFraction = x.coerceIn(widthFraction / 2f, 1f - widthFraction / 2f)
        centerYFraction = y.coerceIn(heightFraction / 2f, 1f - heightFraction / 2f)
        invalidate()
    }

    private fun updateBounds() {
        val w = width * widthFraction
        val h = height * heightFraction
        val cx = width * centerXFraction
        val cy = height * centerYFraction
        bounds.set(cx - w / 2f, cy - h / 2f, cx + w / 2f, cy + h / 2f)
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (!isEditingMode || width <= 0 || height <= 0) return false
        updateBounds()
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                lastX = event.x
                lastY = event.y
                val hit = handleRadius * 2.5f
                resizeCorner = when {
                    kotlin.math.abs(event.x - bounds.left) <= hit && kotlin.math.abs(event.y - bounds.top) <= hit -> 1
                    kotlin.math.abs(event.x - bounds.right) <= hit && kotlin.math.abs(event.y - bounds.top) <= hit -> 2
                    kotlin.math.abs(event.x - bounds.left) <= hit && kotlin.math.abs(event.y - bounds.bottom) <= hit -> 3
                    kotlin.math.abs(event.x - bounds.right) <= hit && kotlin.math.abs(event.y - bounds.bottom) <= hit -> 4
                    else -> 0
                }
                resizing = resizeCorner != 0
                dragging = bounds.contains(event.x, event.y) && !resizing
                return resizing || dragging
            }
            MotionEvent.ACTION_MOVE -> {
                val dx = (event.x - lastX) / width
                val dy = (event.y - lastY) / height
                lastX = event.x
                lastY = event.y
                if (resizing) {
                    var left = bounds.left / width
                    var top = bounds.top / height
                    var right = bounds.right / width
                    var bottom = bounds.bottom / height
                    if (resizeCorner == 1 || resizeCorner == 3) left = (left + dx).coerceIn(0f, right - 0.05f)
                    else right = (right + dx).coerceIn(left + 0.05f, 1f)
                    if (resizeCorner == 1 || resizeCorner == 2) top = (top + dy).coerceIn(0f, bottom - 0.05f)
                    else bottom = (bottom + dy).coerceIn(top + 0.05f, 1f)
                    widthFraction = (right - left).coerceIn(0.05f, 1f)
                    heightFraction = (bottom - top).coerceIn(0.05f, 1f)
                    centerXFraction = (left + right) / 2f
                    centerYFraction = (top + bottom) / 2f
                    invalidate()
                    return true
                }
                if (dragging) {
                    centerXFraction = (centerXFraction + dx).coerceIn(widthFraction / 2f, 1f - widthFraction / 2f)
                    centerYFraction = (centerYFraction + dy).coerceIn(heightFraction / 2f, 1f - heightFraction / 2f)
                    invalidate()
                    return true
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                val handled = dragging || resizing
                if (handled) onBoundsChanged?.invoke(centerXFraction, centerYFraction, widthFraction, heightFraction)
                dragging = false
                resizing = false
                resizeCorner = 0
                performClick()
                return handled
            }
        }
        return dragging || resizing
    }

    override fun performClick(): Boolean {
        super.performClick()
        return true
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (!isEditingMode) return
        updateBounds()
        canvas.drawRoundRect(bounds, 8f, 8f, fillPaint)
        canvas.drawRoundRect(bounds, 8f, 8f, borderPaint)
        val r = handleRadius
        val points = arrayOf(
            bounds.left to bounds.top,
            bounds.right to bounds.top,
            bounds.left to bounds.bottom,
            bounds.right to bounds.bottom
        )
        points.forEach { (x, y) ->
            canvas.drawCircle(x, y, r, handlePaint)
            canvas.drawCircle(x, y, r, borderPaint)
        }
        canvas.drawText("EFFECT", bounds.left + 8f * resources.displayMetrics.density,
            max(bounds.top + 18f * resources.displayMetrics.density, 18f * resources.displayMetrics.density), labelPaint)
    }
}
