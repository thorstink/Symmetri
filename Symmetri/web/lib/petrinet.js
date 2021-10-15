function isEnabled (pre) {
  return _(pre).all(function (multiplicity, place) {
    return (marking[place] || 0) >= multiplicity
  })
}

function updateMarking (remove, add) {
  // don't do anything if it's not enabled
  if (!isEnabled(remove)) {
		return
	}

  _.each(add, function (multiplicity, place) {
    if (!marking[place]) {
      marking[place] = 0
    }

    marking[place] += multiplicity
  })

  _.each(remove, function (multiplicity, place) {
    if (!marking[place]) {
      marking[place] = 0
    }

    marking[place] -= multiplicity
  })

  redraw()
}

function arcs (transitions) {
  return transitions.reduce(function (arcs, t) {
    var incoming = mapLocations(t.pre).map(function (arc) {
      return {
        x1: arc.x,
        y1: arc.y,
        x2: t.x,
        y2: t.y,
        incoming: true,
        weight: arc.weight
      }
    })

    var outgoing = mapLocations(t.post).map(function (arc) {
      return {
        x1: t.x,
        y1: t.y,
        x2: arc.x,
        y2: arc.y,
        incoming: false,
        weight: arc.weight
      }
    })

    return arcs.concat(incoming).concat(outgoing)
  }, [])
}
