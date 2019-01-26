module Shaders = Revery_Shaders;
module Geometry = Revery_Geometry;
module Layout = Layout;
module LayoutTypes = Layout.LayoutTypes;

open Fontkit;
open Reglm;
open Revery_Core;

open ViewNode;
open RenderPass;

let debugImageIndex = ref(0);

let saveDebugImage = pixels => {
  open Bigarray;
  /* open Reglfw; */
  let width = Array2.dim2(pixels);
  let height = Array2.dim1(pixels);
  let debugImagePixels =
    Array2.create(
      int8_unsigned,
      c_layout,
      height,
      width * 4 /* RGBA */
    );
  Array2.fill(debugImagePixels, 0);
  for (y in 0 to height - 1) {
    for (x in 0 to width - 1) {
      Array2.set(debugImagePixels, y, x * 4, Array2.get(pixels, y, x));
      Array2.set(debugImagePixels, y, x * 4 + 1, Array2.get(pixels, y, x));
      Array2.set(debugImagePixels, y, x * 4 + 2, Array2.get(pixels, y, x));
      Array2.set(debugImagePixels, y, x * 4 + 3, Array2.get(pixels, y, x));
    };
  };
  /* let debugImage = Image.create(debugImagePixels); */
  /* Image.save(
       debugImage,
       "debugImage" ++ string_of_int(debugImageIndex^) ++ ".tga",
     ); */
  debugImageIndex := debugImageIndex^ + 1;
};

class textNode (text: string) = {
  as _this;
  val mutable text = text;
  val quad = Assets.quad();
  val fontShader = Assets.fontShader();
  val glyphAtlas = GlyphAtlas.getInstance();
  inherit (class viewNode)() as _super;
  pub! draw = (pass: renderPass, parentContext: NodeDrawContext.t) => {
    /* Draw background first */
    _super#draw(pass, parentContext);

    switch (pass) {
    | AlphaPass(projectionMatrix) =>
      Shaders.CompiledShader.use(fontShader);

      Shaders.CompiledShader.setUniformMatrix4fv(
        fontShader,
        "uProjection",
        projectionMatrix,
      );

      let style = _super#getStyle();
      let opacity = style.opacity *. parentContext.opacity;
      let font =
        FontCache.load(
          style.fontFamily,
          int_of_float(
            float_of_int(style.fontSize) *. parentContext.pixelRatio +. 0.5,
          ),
        );
      let dimensions = _super#measurements();
      let color = Color.multiplyAlpha(opacity, style.color);
      Shaders.CompiledShader.setUniform4fv(
        fontShader,
        "uColor",
        Color.toVec4(color),
      );

      let outerTransform = Mat4.create();
      Mat4.fromTranslation(
        outerTransform,
        Vec3.create(0.0, float_of_int(dimensions.height), 0.0),
      );

      let render = (s: Fontkit.fk_shape, x: float) => {
        let glyph = FontRenderer.getGlyph(font, s.glyphId);
        /* saveDebugImage(glyph.bitmap); */
        let {bitmap, width, height, bearingX, bearingY, advance, _} = glyph;

        let width = float_of_int(width) /. parentContext.pixelRatio;
        let height = float_of_int(height) /. parentContext.pixelRatio;
        let bearingX = float_of_int(bearingX) /. parentContext.pixelRatio;
        let bearingY = float_of_int(bearingY) /. parentContext.pixelRatio;
        let advance = float_of_int(advance) /. parentContext.pixelRatio;

        let atlasGlyph = GlyphAtlas.copyGlyphToAtlas((glyphAtlas, bitmap));

        let atlasOrigin =
          Vec2.create(atlasGlyph.textureU, atlasGlyph.textureV);
        Shaders.CompiledShader.setUniform2fv(
          fontShader,
          "uAtlasOrigin",
          atlasOrigin,
        );
        let atlasSize =
          Vec2.create(atlasGlyph.textureWidth, atlasGlyph.textureHeight);
        Shaders.CompiledShader.setUniform2fv(
          fontShader,
          "uAtlasSize",
          atlasSize,
        );

        let glyphTransform = Mat4.create();
        Mat4.fromTranslation(
          glyphTransform,
          Vec3.create(
            x +. bearingX +. width /. 2.,
            height *. 0.5 -. bearingY,
            0.0,
          ),
        );

        let scaleTransform = Mat4.create();
        Mat4.fromScaling(scaleTransform, Vec3.create(width, height, 1.0));

        let local = Mat4.create();
        Mat4.multiply(local, glyphTransform, scaleTransform);

        let xform = Mat4.create();
        Mat4.multiply(xform, outerTransform, local);
        Mat4.multiply(xform, _this#getWorldTransform(), xform);

        Shaders.CompiledShader.setUniformMatrix4fv(
          fontShader,
          "uWorld",
          xform,
        );

        GlyphAtlas.bindGlyphAtlas(glyphAtlas);
        Geometry.draw(quad, fontShader);

        x +. advance /. 64.0;
      };

      let shapedText = FontRenderer.shape(font, text);
      let startX = ref(0.);
      Array.iter(
        s => {
          let nextPosition = render(s, startX^);
          startX := nextPosition;
        },
        shapedText,
      );
    /* saveDebugImage(glyphAtlas.pixels); */
    | _ => ()
    };
  };
  pub setText = t => text = t;
  pub! getMeasureFunction = pixelRatio => {
    let measure =
        (_mode, _width, _widthMeasureMode, _height, _heightMeasureMode) => {
      /* TODO: Cache font locally in variable */
      let style = _super#getStyle();
      let font =
        FontCache.load(
          style.fontFamily,
          int_of_float(float_of_int(style.fontSize) *. pixelRatio),
        );

      let d = FontRenderer.measure(font, text);
      let ret: Layout.LayoutTypes.dimensions = {
        LayoutTypes.width: int_of_float(float_of_int(d.width) /. pixelRatio),
        height: int_of_float(float_of_int(d.height) /. pixelRatio),
      };
      ret;
    };
    Some(measure);
  };
};