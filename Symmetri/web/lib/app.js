var r = 10
var w = 10
var h = 10

var svg = d3.select('body')
  .append('svg')
  .attr('width', '100%')
  .attr('height', '100%')

// define arrow markers for graph links
svg.append('svg:defs').append('svg:marker')
  .attr({
    id: 'arrow',
    viewBox: '0 -5 10 10',
    refX: 3,
    markerWidth: 7,
    markerHeight: 7,
    orient: 'auto'
  })
  .append('svg:path')
  .attr({
    d: 'M0,-5 L10,0 L0,5',
    fill: 'black'
  })

var places = svg.append('g')

places.selectAll('circle')
  .data(states)
  .enter()
  .append('circle')
  .attr({
    r: r,
    fill: 'rgba(0,255,255,.2)',
    stroke: 'black',
    cx: function (d) { return d.x - 0.5 },
    cy: function (d) { return d.y - 0.5 }
  })

// take a multiset dictionary 'label => multiplicity'
// and return a list of [{x, y, weight}]
function mapLocations (mset) {
  return _.map(mset,
    function (multiplicity, label) {
      var s = _.find(states, 'label', label)
      return {
        x: s.x,
        y: s.y,
        weight: multiplicity
      }
    }
  )
}

// construct SVG line from arc
var arcPathToCenter = function (arc) {
  return 'M ' + arc.x1 + ',' + arc.y1 + 'L ' + arc.x2 + ',' + arc.y2
}

// construct SVG line from arc, trying to be smart about the shapes and length of arc
var arcPath = function (d) {
  var deltaX = d.x2 - d.x1
  var deltaY = d.y2 - d.y1
  var dist = Math.sqrt(deltaX * deltaX + deltaY * deltaY)
  var normX = deltaX / dist
  var normY = deltaY / dist
  var sourcePadding = (d.incoming ? 10 : 14)
  var targetPadding = (d.incoming ? 17 : 15)
  var sourceX = -0.5 + d.x1 + (sourcePadding * normX)
  var sourceY = -0.5 + d.y1 + (sourcePadding * normY)
  var targetX = -0.5 + d.x2 - (targetPadding * normX)
  var targetY = -0.5 + d.y2 - (targetPadding * normY)
  return 'M' + sourceX + ',' + sourceY + 'L' + targetX + ',' + targetY
}

var Arcs = svg.append('g')

Arcs.selectAll('path')
  .data(arcs(transitions))
  .enter()
  .append('path')
  .style('marker-end', 'url(#arrow)')
  .attr({
    d: arcPath,
    stroke: 'black',
    strokeWidth: 1
  })

var transitionG = svg.append('g')

function tokens () {
  return _.map(marking, function (val, key) {
    return {
      state: key,
      tokens: val
    }
  })
}

var tokensG = svg.append('g')

function redrawTransitions () {
  var transitionSvg = transitionG.selectAll('rect')
    .data(transitions)

  // create or update
  transitionSvg.enter()
    .append('rect')
    .attr({
      stroke: 'black',
      width: w * 2,
      height: h * 2
    })
    .on('click', function (t) {
      // transition the marking
      // m_1 = m_0 - (pre t) + (post t)
      updateMarking(t.pre, t.post)
    })

  // update new
  transitionSvg.attr({
    fill: function (d) {
      return isEnabled(d.pre) ? 'rgba(0,0,255,.3)' : 'rgba(0,0,0,.3)'
    },
    cursor: function (d) {
      return isEnabled(d.pre) ? 'pointer' : 'default'
    },
    x: function (d) { return d.x - w - 0.5 },
    y: function (d) { return d.y - h - 0.5 }
  })

  // remove on exit
  transitionSvg.exit().remove()
}

function redrawTokens () {
  var tokenSvg = tokensG
    .selectAll('text')
    .data(tokens())

  tokenSvg.enter()
    .append('text')

  tokenSvg.attr({
    visibility: function (d) {
      return (d.tokens === 0) ? 'hidden' : 'visible'
    },
    x: function (d) {
      return _.find(states, 'label', d.state).x - 0.5
    },
    y: function (d) {
      return _.find(states, 'label', d.state).y - 0.5
    },
    dx: -r / 2 + 0.5,
    dy: r / 2 - 0.5
  })
    .text(function (d) {
      return d.tokens
    })

  tokenSvg.exit()
    .remove()
}

function redraw () {
  redrawTokens()
  redrawTransitions()
}

redraw()
