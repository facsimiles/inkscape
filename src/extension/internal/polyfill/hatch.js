(function () {
  // Name spaces -----------------------------------
  const svgNS = 'http://www.w3.org/2000/svg';
  const xlinkNS = 'http://www.w3.org/1999/xlink';
  const xhtmlNS = 'http://www.w3.org/1999/xhtml';

  // Set multiple attributes to an element
  const setAttributes = (el, attrs) => {
    for (let key in attrs) {
      el.setAttribute(key, attrs[key]);
    }
  };

  // Generate x-axis coordinates for the pattern paths
  const generatePositions = (width, diagonal, initial, distance) => {
    const offset = (diagonal - width) / 2;
    const leftDistance = initial + offset;
    const rightDistance = width + offset + distance;
    const units = Math.round(leftDistance / distance) + 1;
    let array = [];

    for (let i = initial - units * distance; i < rightDistance; i += distance) {
      array.push(i);
    }

    return array;
  };


  // Start of document processing ---------------------
  const svg = document.querySelectorAll('svg')[0];
  const shapes = document.querySelectorAll('rect,circle,ellipse,path,text');

  shapes.forEach((shape, i) => {
    // Get id. If no id, create one.
    let shapeId = shape.getAttribute('id');
    if (!shapeId) {
      shapeId = 'hatch_shape_' + i;
      shape.setAttribute('id', shapeId);
    }

    const fill = shape.getAttribute('fill') || shape.style.fill;
    const fillURL = fill.match(/^url\(\s*"?\s*#([^\s"]+)"?\s*\)/);

    if (fillURL && fillURL[1]) {
      const hatch = document.getElementById(fillURL[1]);

      if (hatch && hatch.nodeName === 'hatch') {
        const bbox = shape.getBBox();
        const x = Number(hatch.getAttribute('x')) || 0;
        const y = Number(hatch.getAttribute('y')) || 0;
        const pitch = Number(hatch.getAttribute('pitch')) || 0;
        const rotate = Number(hatch.getAttribute('rotate')) || 0;
        const hatchUnits = hatch.getAttribute('hatchUnits') || 'objectBoundingBox';
        const hatchContentUnits = hatch.getAttribute('hatchContentUnits') || 'userSpaceOnUse';
        let transform = hatch.getAttribute('transform');

        const hatchpaths = hatch.querySelectorAll('hatchpath,hatchPath');

        // if (hatchUnits === 'objectBoundingBox') {
        //   myMesh.scale(new Point(boxWidth, boxHeight));
        // }

        const pattern = document.createElementNS(svgNS, 'pattern');
        const patternId = `${fillURL[1]}_pattern`;
        setAttributes(pattern, {
          'id': patternId,
          'patternUnits': 'userSpaceOnUse',
          'width': bbox.width,
          'height': bbox.height,
          'x': bbox.x,
          'y': bbox.y
        });
        hatch.parentElement.insertBefore(pattern, hatch);

        const hatchDiagonal = Math.ceil(Math.sqrt(
          bbox.width * bbox.width + bbox.height * bbox.height
        ));

        const xPositions = generatePositions(
          bbox.width, hatchDiagonal, x, pitch
        );

        hatchpaths.forEach((hatchpath) => {
          let offset = Number(hatchpath.getAttribute('offset')) || 0;

          offset = offset > pitch ? (offset % pitch) : offset;
          let currentXPositions = xPositions.map(p => p + offset);

          let path = document.createElementNS(svgNS, 'path');
          let data = hatchpath.getAttribute('d');
          let d = '';

          for (let j = 0; j < hatchpath.attributes.length; ++j) {
            let attr = hatchpath.attributes.item(j);
            if (attr.name !== 'd') {
              path.setAttribute(attr.name, attr.value);
            }
          }

          if (hatchpath.getAttribute('d') === null) {
            currentXPositions.forEach((xPos) => {
              d += `M ${xPos} ${y} V ${hatchDiagonal} `;
            });
          } else {
            // TODO create Point class (x, y)
            // break data into Points (regex parsing)
            // const startsWithM
            // const endsWithZ
            // const connectedEnds = !startsWithM && !endsWithZ;
            // add starting point (0, 0) if missing
            // duplicate d for x in currentXPosition and y [0 - pathHeight, hatchDiagonal]
          }

          path.setAttribute('d', d);
          pattern.appendChild(path);
        });

        shape.style.fill = `url(#${patternId})`;
        shape.setAttribute('fill', `url(#${patternId})`);
      }
    }
  });
})();
