// SPDX-License-Identifier: GPL-2.0-or-later
// Use Canvas to render a mesh gradient, passing the rendering to an image via a data stream.
// Copyright: Tavmjong Bah 2018
// Distributed under GNU General Public License version 2 or later. See <http://fsf.org/>.

(function () {
  // Name spaces -----------------------------------
  const svgNS = 'http://www.w3.org/2000/svg';
  const xlinkNS = 'http://www.w3.org/1999/xlink';
  const xhtmlNS = 'http://www.w3.org/1999/xhtml';
  /*
   * Maximum threshold for Bezier step size
   * Larger values leave holes, smaller take longer to render.
   */
  const maxBezierStep = 2.0;

  // Test if mesh gradients are supported.
  if (document.createElementNS(svgNS, 'meshgradient').x) {
    return;
  }

  /*
   * Utility functions -----------------------------
   */
  const colorToString = (c) =>
    `rgb(${Math.round(c[0])},${Math.round(c[1])},${Math.round(c[2])})`;

  // Split Bezier using de Casteljau's method.
  const splitBezier = (p0, p1, p2, p3) => {
    let tmp = new Point((p1.x + p2.x) * 0.5, (p1.y + p2.y) * 0.5);
    let p01 = new Point((p0.x + p1.x) * 0.5, (p0.y + p1.y) * 0.5);
    let p12 = new Point((p2.x + p3.x) * 0.5, (p2.y + p3.y) * 0.5);
    let p02 = new Point((tmp.x + p01.x) * 0.5, (tmp.y + p01.y) * 0.5);
    let p11 = new Point((tmp.x + p12.x) * 0.5, (tmp.y + p12.y) * 0.5);
    let p03 = new Point((p02.x + p11.x) * 0.5, (p02.y + p11.y) * 0.5);

    let p10 = p03.clone();
    let p00 = p0.clone();
    let p13 = p3.clone();

    return ([
      [p00, p01, p02, p03],
      [p10, p11, p12, p13]
    ]);
  };

  // See Cairo: cairo-mesh-pattern-rasterizer.c
  const bezierStepsSquared = (points) => {
    let tmp0 = points[0].distSquared(points[1]);
    let tmp1 = points[2].distSquared(points[3]);
    let tmp2 = points[0].distSquared(points[2]) * 0.25;
    let tmp3 = points[1].distSquared(points[3]) * 0.25;

    let max1 = (tmp0 > tmp1 ? tmp0 : tmp1);

    let max2 = (tmp2 > tmp3 ? tmp2 : tmp3);

    let max = (max1 > max2 ? max1 : max2);

    return max * 18;
  };

  // Weighted average to find Bezier points for linear sides.
  const wAvg = (p0, p1) => p0.scale(2.0 / 3.0).add(p1.scale(1.0 / 3.0));

  // Browsers return a string rather than a transform list for gradientTransform!
  const parseTransform = (t) => {
    // console.log( "parseTransform: " + t );
    let affine = new Affine();
    let trans, scale, radian, tan, skewx, skewy, rotate;
    let transforms = t.match(/(\w+\(\s*[^)]+\))+/g);

    transforms.forEach((i) => {
      let c = i.match(/[\w.-]+/g);
      let type = c.shift();

      switch (type) {
        case 'translate':
          if (c.length === 2) {
            trans = new Affine(1, 0, 0, 1, c[0], c[1]);
          } else {
            console.error('mesh.js: translate does not have 2 arguments!');
            trans = new Affine(1, 0, 0, 1, 0, 0);
          }
          affine = affine.append(trans);
          break;

        case 'scale':
          if (c.length === 1) {
            scale = new Affine(c[0], 0, 0, c[0], 0, 0);
          } else if (c.length === 2) {
            scale = new Affine(c[0], 0, 0, c[1], 0, 0);
          } else {
            console.error('mesh.js: scale does not have 1 or 2 arguments!');
            scale = new Affine(1, 0, 0, 1, 0, 0);
          }
          affine = affine.append(scale);
          break;

        case 'rotate':
          if (c.length === 3) {
            trans = new Affine(1, 0, 0, 1, c[1], c[2]);
            affine = affine.append(trans);
          }
          if (c[0]) {
            radian = c[0] * Math.PI / 180.0;
            let cos = Math.cos(radian);
            let sin = Math.sin(radian);
            if (Math.abs(cos) < 1e-16) { // I hate rounding errors...
              cos = 0;
            }
            if (Math.abs(sin) < 1e-16) { // I hate rounding errors...
              sin = 0;
            }
            rotate = new Affine(cos, sin, -sin, cos, 0, 0);
            affine = affine.append(rotate);
          } else {
            console.error('math.js: No argument to rotate transform!');
          }
          if (c.length === 3) {
            trans = new Affine(1, 0, 0, 1, -c[1], -c[2]);
            affine = affine.append(trans);
          }
          break;

        case 'skewX':
          if (c[0]) {
            radian = c[0] * Math.PI / 180.0;
            tan = Math.tan(radian);
            skewx = new Affine(1, 0, tan, 1, 0, 0);
            affine = affine.append(skewx);
          } else {
            console.error('math.js: No argument to skewX transform!');
          }
          break;

        case 'skewY':
          if (c[0]) {
            radian = c[0] * Math.PI / 180.0;
            tan = Math.tan(radian);
            skewy = new Affine(1, tan, 0, 1, 0, 0);
            affine = affine.append(skewy);
          } else {
            console.error('math.js: No argument to skewY transform!');
          }
          break;

        case 'matrix':
          if (c.length === 6) {
            affine = affine.append(new Affine(...c));
          } else {
            console.error('math.js: Incorrect number of arguments for matrix!');
          }
          break;

        default:
          console.error('mesh.js: Unhandled transform type: ' + type);
          break;
      }
    });
    // console.log( "  affine:\n" + affine.toString() );
    return affine;
  };

  const parsePoints = (s) => {
    let points = [];
    let values = s.split(/[ ,]+/);
    for (let i = 0, imax = values.length - 1; i < imax; i += 2) {
      points.push(new Point(parseFloat(values[i]), parseFloat(values[i + 1])));
    }
    return points;
  };

  // Point class -----------------------------------
  class Point {
    constructor (x, y) {
      this.x = x || 0;
      this.y = y || 0;
    }

    toString () {
      return `(x=${this.x}, y=${this.y})`;
    }

    clone () {
      return new Point(this.x, this.y);
    }

    add (v) {
      return new Point(this.x + v.x, this.y + v.y);
    }

    scale (v) {
      if (v.x === undefined) {
        return new Point(this.x * v, this.y * v);
      }
      return new Point(this.x * v.x, this.y * v.y);
    }

    distSquared (v) {
      let x = this.x - v.x;
      let y = this.y - v.y;
      return (x * x + y * y);
    }

    // Transform by affine
    transform (affine) {
      let x = this.x * affine.a + this.y * affine.c + affine.e;
      let y = this.x * affine.b + this.y * affine.d + affine.f;
      return new Point(x, y);
    }
  }

  /*
   * Affine class -------------------------------------
   *
   * As defined in the SVG spec
   * | a  c  e |
   * | b  d  f |
   * | 0  0  1 |
   *
   */

  class Affine {
    constructor (a, b, c, d, e, f) {
      if (a === undefined) {
        this.a = 1;
        this.b = 0;
        this.c = 0;
        this.d = 1;
        this.e = 0;
        this.f = 0;
      } else {
        this.a = a;
        this.b = b;
        this.c = c;
        this.d = d;
        this.e = e;
        this.f = f;
      }
    }

    toString () {
      return `affine: ${this.a} ${this.c} ${this.e} \n\
       ${this.b} ${this.d} ${this.f}`;
    }

    append (v) {
      if (!(v instanceof Affine)) {
        console.error(`mesh.js: argument to Affine.append is not affine!`);
      }
      let a = this.a * v.a + this.c * v.b;
      let b = this.b * v.a + this.d * v.b;
      let c = this.a * v.c + this.c * v.d;
      let d = this.b * v.c + this.d * v.d;
      let e = this.a * v.e + this.c * v.f + this.e;
      let f = this.b * v.e + this.d * v.f + this.f;
      return new Affine(a, b, c, d, e, f);
    }
  }

  // Curve class --------------------------------------
  class Curve {
    constructor (nodes, colors) {
      this.nodes = nodes; // 4 Bezier points
      this.colors = colors; // 2 x 4 colors (two ends x R+G+B+A)
    }

    /*
     * Paint a Bezier curve
     * w is canvas.width
     * h is canvas.height
     */
    paintCurve (v, w, h) {
      // If inside, see if we need to split
      if (bezierStepsSquared(this.nodes) > maxBezierStep) {
        const beziers = splitBezier(
          this.nodes[0], this.nodes[1],
          this.nodes[2], this.nodes[3]
        );
        // ([start][end])
        let colors0 = [[],[]];
        let colors1 = [[],[]];

        /*
         * TODO turn into cubic 1D interpolation of the midpoint
         * Linear horizontal interpolation of the middle value for every
         * patch exceeding thereshold
         */
        for (let i = 0; i < 4; ++i) {
          colors0[0][i] = this.colors[0][i];
          colors0[1][i] = (this.colors[0][i] + this.colors[1][i]) / 2;
          colors1[0][i] = (this.colors[0][i] + this.colors[1][i]) / 2;
          colors1[1][i] = this.colors[1][i];
        }
        let curve0 = new Curve(beziers[0], colors0);
        let curve1 = new Curve(beziers[1], colors1);
        curve0.paintCurve(v, w);
        curve1.paintCurve(v, w);
      } else {
        // Directly write data
        let x = Math.round(this.nodes[0].x);
        let y = Math.round(this.nodes[0].y);
        if (x >= 0 && x < w) {
          let index = (y * w + x) * 4;
          v[index] = Math.round(this.colors[0][0]);
          v[index + 1] = Math.round(this.colors[0][1]);
          v[index + 2] = Math.round(this.colors[0][2]);
          v[index + 3] = Math.round(this.colors[0][3]); // Alpha
        }
      }
    }
  }

  // Patch class -------------------------------------
  class Patch {
    constructor (nodes, colors) {
      this.nodes = nodes; // 4x4 array of points
      this.colors = colors; // 2x2x4 colors (four corners x R+G+B+A)
    }

    // Set path for future stroking or filling... useful for debugging.
    setOutline (v) {
      // Draw patch outline
      v.beginPath();
      v.moveTo(this.nodes[0][0].x, this.nodes[0][0].y);
      v.bezierCurveTo(this.nodes[0][1].x, this.nodes[0][1].y,
        this.nodes[0][2].x, this.nodes[0][2].y,
        this.nodes[0][3].x, this.nodes[0][3].y);
      v.bezierCurveTo(this.nodes[1][3].x, this.nodes[1][3].y,
        this.nodes[2][3].x, this.nodes[2][3].y,
        this.nodes[3][3].x, this.nodes[3][3].y);
      v.bezierCurveTo(this.nodes[3][2].x, this.nodes[3][2].y,
        this.nodes[3][1].x, this.nodes[3][1].y,
        this.nodes[3][0].x, this.nodes[3][0].y);
      v.bezierCurveTo(this.nodes[2][0].x, this.nodes[2][0].y,
        this.nodes[1][0].x, this.nodes[1][0].y,
        this.nodes[0][0].x, this.nodes[0][0].y);
      v.closePath();
    }

    // Draw stroke patch... useful if debugging.
    drawOutline (v) {
      this.setOutline(v);
      v.strokeStyle = 'black';
      v.stroke();
    }

    // Fill patch... useful if debugging.
    fillOutline (v) {
      this.setOutline(v);
      v.fillStyle = colorToString(this.colors[0]);
      v.fill();
    }

    // Split patch horizontally into two patches.
    split () {
      // console.log( "Patch.split" );

      let nodes0 = [[],[],[],[]];
      let nodes1 = [[],[],[],[]];
      let colors0 = [
        [[],[]],
        [[],[]]
      ];
      let colors1 = [
        [[],[]],
        [[],[]]
      ];

      for (let i = 0; i < 4; ++i) {
        const beziers = splitBezier(
          this.nodes[0][i], this.nodes[1][i],
          this.nodes[2][i], this.nodes[3][i]
        );

        nodes0[0][i] = beziers[0][0];
        nodes0[1][i] = beziers[0][1];
        nodes0[2][i] = beziers[0][2];
        nodes0[3][i] = beziers[0][3];
        nodes1[0][i] = beziers[1][0];
        nodes1[1][i] = beziers[1][1];
        nodes1[2][i] = beziers[1][2];
        nodes1[3][i] = beziers[1][3];
      }

      /*
       * TODO turn into cubic 1D interpolation of the midpoint
       * Linear vertical interpolation of the middle value for every
       * patch exceeding thereshold
       */
      for (let i = 0; i < 4; ++i) {
        colors0[0][0][i] = this.colors[0][0][i];
        colors0[0][1][i] = this.colors[0][1][i];
        colors0[1][0][i] = (this.colors[0][0][i] + this.colors[1][0][i]) / 2;
        colors0[1][1][i] = (this.colors[0][1][i] + this.colors[1][1][i]) / 2;
        colors1[0][0][i] = (this.colors[0][0][i] + this.colors[1][0][i]) / 2;
        colors1[0][1][i] = (this.colors[0][1][i] + this.colors[1][1][i]) / 2;
        colors1[1][0][i] = this.colors[1][0][i];
        colors1[1][1][i] = this.colors[1][1][i];
      }

      return ([new Patch(nodes0, colors0), new Patch(nodes1, colors1)]);
    }

    paint (v, w, h) {
      // console.log( "Patch.paint" );
      // console.log( this.nodes );

      // Check if patch is inside canvas
      if (this.nodes[3][3].x < 0 || this.nodes[0][0].x > w
        || this.nodes[3][3].y < 0 || this.nodes[0][0].y > h) {
        return;
      }

      // If inside, see if we need to split
      let larger = false;
      let step;
      for (let i = 0; i < 4; ++i) {
        step = bezierStepsSquared([
          this.nodes[0][i], this.nodes[1][i],
          this.nodes[2][i], this.nodes[3][i]
        ]);

        if (step > maxBezierStep) {
          larger = true;
          break;
        }
      }

      if (larger) {
        // console.log( "Paint: Splitting" );
        // TODO split into two patches using cubic interpolation
        let patches = this.split();
        patches[0].paint(v, w, h);
        patches[1].paint(v, w, h);
      } else {
        // console.log( "Paint: Filling" );
        this.paintCurve(v, w, h);
      }
    }

    paintCurve (v, w) {
      // console.log( "Patch.paintCurve" );

      // Paint a Bezier curve using just the top of the patch. If
      // the patch is thin enough this should work. We leave this
      // function here in case we want to do something more fancy.
      let curve = new Curve([...this.nodes[0]], [...this.colors[0]]);
      curve.paintCurve(v, w);
    }
  }

  // Mesh class ---------------------------------------
  class Mesh {
    constructor (id) {
      // console.log( "Mesh: " + id );
      this.id = id;
      this.readMesh(id);
      // console.log( this.nodes );
      // console.log( this.colors );
    }

    // Function to parse an SVG mesh and set the nodes (points) and colors
    readMesh (id) {
      let nodes = [];
      let colors = [];

      // First, find the mesh
      let theMesh = document.getElementById(id);
      if (theMesh == null) {
        console.error('mesh.js: Could not find mesh: ' + id);
      } else {
        // console.log( "Reading mesh: " + id);

        nodes[0] = []; // Top row
        colors[0] = []; // Top row

        let x = Number(theMesh.getAttribute('x'));
        let y = Number(theMesh.getAttribute('y'));
        nodes[0][0] = new Point(x, y);

        let rows = theMesh.children;
        for (let i = 0, imax = rows.length; i < imax; ++i) {
          // Need to validate if meshrow...
          nodes[3 * i + 1] = []; // Need three extra rows for each meshrow.
          nodes[3 * i + 2] = [];
          nodes[3 * i + 3] = [];
          colors[i + 1] = []; // Need one more row than number of meshrows.
          // console.log( " row: " + i);
          let patches = rows[i].children;
          for (let j = 0, jmax = patches.length; j < jmax; ++j) {
            // console.log( "  patch: " + j);
            let stops = patches[j].children;
            for (let k = 0, kmax = stops.length; k < kmax; ++k) {
              let l = k;
              if (i !== 0) {
                ++l; // There is no top if row isn't first row.
              }
              // console.log( "   stop: " + k);
              let path = stops[k].getAttribute('path');
              let parts;

              let type = 'l'; // We need to still find mid-points even if no path.
              if (path != null) {
                parts = path.match(/\s*([lLcC])\s*(.*)/);
                type = parts[1];
              }
              let stopNodes = parsePoints(parts[2]);

              switch (type) {
                case 'l':
                  if (l === 0) { // Top
                    nodes[3 * i][3 * j + 3] = stopNodes[0].add(nodes[3 * i][3 * j]);
                    nodes[3 * i][3 * j + 1] = wAvg(nodes[3 * i][3 * j], nodes[3 * i][3 * j + 3]);
                    nodes[3 * i][3 * j + 2] = wAvg(nodes[3 * i][3 * j + 3], nodes[3 * i][3 * j]);
                  } else if (l === 1) { // Right
                    nodes[3 * i + 3][3 * j + 3] = stopNodes[0].add(nodes[3 * i][3 * j + 3]);
                    nodes[3 * i + 1][3 * j + 3] = wAvg(nodes[3 * i][3 * j + 3], nodes[3 * i + 3][3 * j + 3]);
                    nodes[3 * i + 2][3 * j + 3] = wAvg(nodes[3 * i + 3][3 * j + 3], nodes[3 * i][3 * j + 3]);
                  } else if (l === 2) { // Bottom
                    if (j === 0) {
                      nodes[3 * i + 3][3 * j + 0] = stopNodes[0].add(nodes[3 * i + 3][3 * j + 3]);
                    }
                    nodes[3 * i + 3][3 * j + 1] = wAvg(nodes[3 * i + 3][3 * j], nodes[3 * i + 3][3 * j + 3]);
                    nodes[3 * i + 3][3 * j + 2] = wAvg(nodes[3 * i + 3][3 * j + 3], nodes[3 * i + 3][3 * j]);
                  } else { // Left
                    nodes[3 * i + 1][3 * j] = wAvg(nodes[3 * i][3 * j], nodes[3 * i + 3][3 * j]);
                    nodes[3 * i + 2][3 * j] = wAvg(nodes[3 * i + 3][3 * j], nodes[3 * i][3 * j]);
                  }
                  break;
                case 'L':
                  if (l === 0) { // Top
                    nodes[3 * i][3 * j + 3] = stopNodes[0];
                    nodes[3 * i][3 * j + 1] = wAvg(nodes[3 * i][3 * j], nodes[3 * i][3 * j + 3]);
                    nodes[3 * i][3 * j + 2] = wAvg(nodes[3 * i][3 * j + 3], nodes[3 * i][3 * j]);
                  } else if (l === 1) { // Right
                    nodes[3 * i + 3][3 * j + 3] = stopNodes[0];
                    nodes[3 * i + 1][3 * j + 3] = wAvg(nodes[3 * i][3 * j + 3], nodes[3 * i + 3][3 * j + 3]);
                    nodes[3 * i + 2][3 * j + 3] = wAvg(nodes[3 * i + 3][3 * j + 3], nodes[3 * i][3 * j + 3]);
                  } else if (l === 2) { // Bottom
                    if (j === 0) {
                      nodes[3 * i + 3][3 * j + 0] = stopNodes[0];
                    }
                    nodes[3 * i + 3][3 * j + 1] = wAvg(nodes[3 * i + 3][3 * j], nodes[3 * i + 3][3 * j + 3]);
                    nodes[3 * i + 3][3 * j + 2] = wAvg(nodes[3 * i + 3][3 * j + 3], nodes[3 * i + 3][3 * j]);
                  } else { // Left
                    nodes[3 * i + 1][3 * j] = wAvg(nodes[3 * i][3 * j], nodes[3 * i + 3][3 * j]);
                    nodes[3 * i + 2][3 * j] = wAvg(nodes[3 * i + 3][3 * j], nodes[3 * i][3 * j]);
                  }
                  break;
                case 'c':
                  if (l === 0) { // Top
                    nodes[3 * i][3 * j + 1] = stopNodes[0].add(nodes[3 * i][3 * j]);
                    nodes[3 * i][3 * j + 2] = stopNodes[1].add(nodes[3 * i][3 * j]);
                    nodes[3 * i][3 * j + 3] = stopNodes[2].add(nodes[3 * i][3 * j]);
                  } else if (l === 1) { // Right
                    nodes[3 * i + 1][3 * j + 3] = stopNodes[0].add(nodes[3 * i][3 * j + 3]);
                    nodes[3 * i + 2][3 * j + 3] = stopNodes[1].add(nodes[3 * i][3 * j + 3]);
                    nodes[3 * i + 3][3 * j + 3] = stopNodes[2].add(nodes[3 * i][3 * j + 3]);
                  } else if (l === 2) { // Bottom
                    nodes[3 * i + 3][3 * j + 2] = stopNodes[0].add(nodes[3 * i + 3][3 * j + 3]);
                    nodes[3 * i + 3][3 * j + 1] = stopNodes[1].add(nodes[3 * i + 3][3 * j + 3]);
                    if (j === 0) {
                      nodes[3 * i + 3][3 * j + 0] = stopNodes[2].add(nodes[3 * i + 3][3 * j + 3]);
                    }
                  } else { // Left
                    nodes[3 * i + 2][3 * j] = stopNodes[0].add(nodes[3 * i + 3][3 * j]);
                    nodes[3 * i + 1][3 * j] = stopNodes[1].add(nodes[3 * i + 3][3 * j]);
                  }
                  break;
                case 'C':
                  if (l === 0) { // Top
                    nodes[3 * i][3 * j + 1] = stopNodes[0];
                    nodes[3 * i][3 * j + 2] = stopNodes[1];
                    nodes[3 * i][3 * j + 3] = stopNodes[2];
                  } else if (l === 1) { // Right
                    nodes[3 * i + 1][3 * j + 3] = stopNodes[0];
                    nodes[3 * i + 2][3 * j + 3] = stopNodes[1];
                    nodes[3 * i + 3][3 * j + 3] = stopNodes[2];
                  } else if (l === 2) { // Bottom
                    nodes[3 * i + 3][3 * j + 2] = stopNodes[0];
                    nodes[3 * i + 3][3 * j + 1] = stopNodes[1];
                    if (j === 0) {
                      nodes[3 * i + 3][3 * j + 0] = stopNodes[2];
                    }
                  } else { // Left
                    nodes[3 * i + 2][3 * j] = stopNodes[0];
                    nodes[3 * i + 1][3 * j] = stopNodes[1];
                  }
                  break;
                default:
                  console.error('mesh.js: ' + type + ' invalid path type.');
              }

              if ((i === 0 && j === 0) || k > 0) {
                let colorRaw = window.getComputedStyle(stops[k])
                  .stopColor.match(/^rgb\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)$/i);
                let alphaRaw = window.getComputedStyle(stops[k])
                  .stopOpacity;
                // console.log( "   colorRaw: " + colorRaw + " alphaRaw: " + alphaRaw);
                let alpha = 255;
                if (alphaRaw) {
                  alpha = parseInt(alphaRaw * 255);
                }
                // console.log( "   alpha: " + alpha );
                if (colorRaw) {
                  if (l === 0) { // upper left corner
                    colors[i][j] = [];
                    colors[i][j][0] = parseInt(colorRaw[1]);
                    colors[i][j][1] = parseInt(colorRaw[2]);
                    colors[i][j][2] = parseInt(colorRaw[3]);
                    colors[i][j][3] = alpha; // Alpha
                  } else if (l === 1) { // upper right corner
                    colors[i][j + 1] = [];
                    colors[i][j + 1][0] = parseInt(colorRaw[1]);
                    colors[i][j + 1][1] = parseInt(colorRaw[2]);
                    colors[i][j + 1][2] = parseInt(colorRaw[3]);
                    colors[i][j + 1][3] = alpha; // Alpha
                  } else if (l === 2) { // lower right corner
                    colors[i + 1][j + 1] = [];
                    colors[i + 1][j + 1][0] = parseInt(colorRaw[1]);
                    colors[i + 1][j + 1][1] = parseInt(colorRaw[2]);
                    colors[i + 1][j + 1][2] = parseInt(colorRaw[3]);
                    colors[i + 1][j + 1][3] = alpha; // Alpha
                  } else if (l === 3) { // lower left corner
                    colors[i + 1][j] = [];
                    colors[i + 1][j][0] = parseInt(colorRaw[1]);
                    colors[i + 1][j][1] = parseInt(colorRaw[2]);
                    colors[i + 1][j][2] = parseInt(colorRaw[3]);
                    colors[i + 1][j][3] = alpha; // Alpha
                  }
                }
              }
            }

            // SVG doesn't use tensor points but we need them for rendering.
            nodes[3 * i + 1][3 * j + 1] = new Point();
            nodes[3 * i + 1][3 * j + 2] = new Point();
            nodes[3 * i + 2][3 * j + 1] = new Point();
            nodes[3 * i + 2][3 * j + 2] = new Point();

            nodes[3 * i + 1][3 * j + 1].x =
              (-4.0 * nodes[3 * i][3 * j].x +
                6.0 * (nodes[3 * i][3 * j + 1].x + nodes[3 * i + 1][3 * j].x) +
                -2.0 * (nodes[3 * i][3 * j + 3].x + nodes[3 * i + 3][3 * j].x) +
                3.0 * (nodes[3 * i + 3][3 * j + 1].x + nodes[3 * i + 1][3 * j + 3].x) +
                -1.0 * nodes[3 * i + 3][3 * j + 3].x) / 9.0;
            nodes[3 * i + 1][3 * j + 2].x =
              (-4.0 * nodes[3 * i][3 * j + 3].x +
                6.0 * (nodes[3 * i][3 * j + 2].x + nodes[3 * i + 1][3 * j + 3].x) +
                -2.0 * (nodes[3 * i][3 * j].x + nodes[3 * i + 3][3 * j + 3].x) +
                3.0 * (nodes[3 * i + 3][3 * j + 2].x + nodes[3 * i + 1][3 * j].x) +
                -1.0 * nodes[3 * i + 3][3 * j].x) / 9.0;
            nodes[3 * i + 2][3 * j + 1].x =
              (-4.0 * nodes[3 * i + 3][3 * j].x +
                6.0 * (nodes[3 * i + 3][3 * j + 1].x + nodes[3 * i + 2][3 * j].x) +
                -2.0 * (nodes[3 * i + 3][3 * j + 3].x + nodes[3 * i][3 * j].x) +
                3.0 * (nodes[3 * i][3 * j + 1].x + nodes[3 * i + 2][3 * j + 3].x) +
                -1.0 * nodes[3 * i][3 * j + 3].x) / 9.0;
            nodes[3 * i + 2][3 * j + 2].x =
              (-4.0 * nodes[3 * i + 3][3 * j + 3].x +
                6.0 * (nodes[3 * i + 3][3 * j + 2].x + nodes[3 * i + 2][3 * j + 3].x) +
                -2.0 * (nodes[3 * i + 3][3 * j].x + nodes[3 * i][3 * j + 3].x) +
                3.0 * (nodes[3 * i][3 * j + 2].x + nodes[3 * i + 2][3 * j].x) +
                -1.0 * nodes[3 * i][3 * j].x) / 9.0;

            nodes[3 * i + 1][3 * j + 1].y =
              (-4.0 * nodes[3 * i][3 * j].y +
                6.0 * (nodes[3 * i][3 * j + 1].y + nodes[3 * i + 1][3 * j].y) +
                -2.0 * (nodes[3 * i][3 * j + 3].y + nodes[3 * i + 3][3 * j].y) +
                3.0 * (nodes[3 * i + 3][3 * j + 1].y + nodes[3 * i + 1][3 * j + 3].y) +
                -1.0 * nodes[3 * i + 3][3 * j + 3].y) / 9.0;
            nodes[3 * i + 1][3 * j + 2].y =
              (-4.0 * nodes[3 * i][3 * j + 3].y +
                6.0 * (nodes[3 * i][3 * j + 2].y + nodes[3 * i + 1][3 * j + 3].y) +
                -2.0 * (nodes[3 * i][3 * j].y + nodes[3 * i + 3][3 * j + 3].y) +
                3.0 * (nodes[3 * i + 3][3 * j + 2].y + nodes[3 * i + 1][3 * j].y) +
                -1.0 * nodes[3 * i + 3][3 * j].y) / 9.0;
            nodes[3 * i + 2][3 * j + 1].y =
              (-4.0 * nodes[3 * i + 3][3 * j].y +
                6.0 * (nodes[3 * i + 3][3 * j + 1].y + nodes[3 * i + 2][3 * j].y) +
                -2.0 * (nodes[3 * i + 3][3 * j + 3].y + nodes[3 * i][3 * j].y) +
                3.0 * (nodes[3 * i][3 * j + 1].y + nodes[3 * i + 2][3 * j + 3].y) +
                -1.0 * nodes[3 * i][3 * j + 3].y) / 9.0;
            nodes[3 * i + 2][3 * j + 2].y =
              (-4.0 * nodes[3 * i + 3][3 * j + 3].y +
                6.0 * (nodes[3 * i + 3][3 * j + 2].y + nodes[3 * i + 2][3 * j + 3].y) +
                -2.0 * (nodes[3 * i + 3][3 * j].y + nodes[3 * i][3 * j + 3].y) +
                3.0 * (nodes[3 * i][3 * j + 2].y + nodes[3 * i + 2][3 * j].y) +
                -1.0 * nodes[3 * i][3 * j].y) / 9.0;
          }
        }
        // console.log( nodes );
      }
      this.nodes = nodes; // (m*3+1) x (n*3+1) points
      this.colors = colors; // (m+1) x (n+1) x 4  colors (R+G+B+A)
    }

    // Extracts out each patch and then paints it
    paintMesh (v, w, h) {
      for (let i = 0, imax = (this.nodes.length - 1) / 3; i < imax; ++i) {
        for (let j = 0, jmax = (this.nodes[0].length - 1) / 3; j < jmax; ++j) {
          let sliceNodes = [];
          for (let k = i * 3, kmax = (i * 3) + 4; k < kmax; ++k) {
            sliceNodes.push(this.nodes[k].slice(j * 3, (j * 3) + 4));
          }

          let sliceColors = [];
          sliceColors.push(this.colors[i].slice(j, j + 2));
          sliceColors.push(this.colors[i + 1].slice(j, j + 2));

          let patch = new Patch(sliceNodes, sliceColors);
          patch.paint(v, w, h);
        }
      }
    }

    // Transforms mesh into coordinate space of canvas (t is either Point or Affine).
    transform (t) {
      if (t instanceof Point) {
        for (let i = 0, imax = this.nodes.length; i < imax; ++i) {
          for (let j = 0, jmax = this.nodes[0].length; j < jmax; ++j) {
            this.nodes[i][j] = this.nodes[i][j].add(t);
          }
        }
      } else if (t instanceof Affine) {
        for (let i = 0, imax = this.nodes.length; i < imax; ++i) {
          for (let j = 0, jmax = this.nodes[0].length; j < jmax; ++j) {
            this.nodes[i][j] = this.nodes[i][j].transform(t);
          }
        }
      }
    }

    // Scale mesh into coordinate space of canvas (t is a Point).
    scale (t) {
      for (let i = 0, imax = this.nodes.length; i < imax; ++i) {
        for (let j = 0, jmax = this.nodes[0].length; j < jmax; ++j) {
          this.nodes[i][j] = this.nodes[i][j].scale(t);
        }
      }
    }
  }

  // Start of document processing ---------------------

  const shapes = document.querySelectorAll('rect,circle,ellipse,path,text');
  // console.log("Shapes: " + shapes.length);

  shapes.forEach((shape, i) => {
    // console.log( shape.nodeName );
    // Get id. If no id, create one.
    let shapeId = shape.getAttribute('id');
    if (!shapeId) {
      shapeId = 'patchjs_shape' + i;
      shape.setAttribute('id', shapeId);
    }

    let fillURL = shape.style.fill.match(/^url\(\s*"?\s*#([^\s"]+)"?\s*\)/);

    if (fillURL && fillURL[1]) {
      // console.log( "Got url! " + fillURL[1]);
      let mesh = document.getElementById(fillURL[1]);
      // console.log( mesh );
      // console.log( mesh.nodeName );
      if (mesh.nodeName === 'meshgradient') {
        let bbox = shape.getBBox();

        // Create temporary canvas
        let myCanvas = document.createElementNS(xhtmlNS, 'canvas');
        myCanvas.width = bbox.width;
        myCanvas.height = bbox.height;

        let myContext = myCanvas.getContext('2d');
        let myCanvasImage = myContext.createImageData(myCanvas.width, myCanvas.height);

        // Draw a mesh
        let myMesh = new Mesh(fillURL[1]);

        // Adjust for bounding box if necessary.
        if (mesh.getAttribute('gradientUnits') === 'objectBoundingBox') {
          myMesh.scale(new Point(bbox.width, bbox.height));
        }

        // Apply gradient transform.
        let gradientTransform = mesh.getAttribute('gradientTransform');
        if (gradientTransform != null) {
          let affine = parseTransform(gradientTransform);
          myMesh.transform(affine);
        }

        // Position to Canvas coordinate.
        if (mesh.getAttribute('gradientUnits') === 'userSpaceOnUse') {
          myMesh.transform(new Point(-bbox.x, -bbox.y));
        }

        // Paint
        myMesh.paintMesh(myCanvasImage.data, myCanvas.width, myCanvas.height);
        myContext.putImageData(myCanvasImage, 0, 0);

        // Create image element of correct size
        let myImage = document.createElementNS(svgNS, 'image');
        myImage.setAttribute('width', myCanvas.width);
        myImage.setAttribute('height', myCanvas.height);
        myImage.setAttribute('x', bbox.x);
        myImage.setAttribute('y', bbox.y);

        // Set image to data url
        let myPNG = myCanvas.toDataURL();
        myImage.setAttributeNS(xlinkNS, 'xlink:href', myPNG);

        // Insert image into document
        shape.parentNode.insertBefore(myImage, shape);
        shape.style.fill = 'none';

        // Create clip referencing shape and insert into document
        let clip = document.createElementNS(svgNS, 'clipPath');
        let clipId = 'patchjs_clip' + i;
        clip.setAttribute('id', clipId);
        let use = document.createElementNS(svgNS, 'use');
        use.setAttributeNS(xlinkNS, 'xlink:href', '#' + shapeId);
        clip.appendChild(use);
        shape.parentElement.insertBefore(clip, shape);
        myImage.setAttribute('clip-path', 'url(#' + clipId + ')');

        // Force the Garbage Collector to free the space
        myCanvasImage = null;
        myCanvas = null;
        myPNG = null;
      }
    }
  });
})();
