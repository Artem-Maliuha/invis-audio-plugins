#include "InvisConstellation.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace invis::ui {

InvisConstellation::InvisConstellation()
{
    setRepaintsOnMouseActivity(false); // hover is tracked explicitly; blanket repaints are wasteful
}

ConstellationMetrics InvisConstellation::getMetrics(InvisConstellationSize size)
{
    //        bezel  corner  star   obs   arc    arcW  poly  label
    switch (size)
    {
        case InvisConstellationSize::S: return { 5.0f,  6.0f,  6.0f, 5.0f, 11.0f, 2.0f, 1.2f,  8.5f };
        case InvisConstellationSize::L: return { 8.0f, 10.0f, 10.0f, 8.5f, 18.0f, 3.0f, 2.0f, 11.5f };
        case InvisConstellationSize::M:
        default:                        return { 6.0f,  8.0f,  8.0f, 7.0f, 14.0f, 2.4f, 1.6f, 10.0f };
    }
}

juce::Point<int> InvisConstellation::getIntrinsicSize(InvisConstellationSize size)
{
    switch (size)
    {
        case InvisConstellationSize::S: return { 300, 300 };
        case InvisConstellationSize::L: return { 520, 520 };
        case InvisConstellationSize::M:
        default:                        return { 400, 400 };
    }
}

// --- Geometry ---------------------------------------------------------------------------------

juce::Rectangle<float> InvisConstellation::getPadArea() const
{
    const auto m = getMetrics(padSize);
    return centreIntrinsic(getLocalBounds().toFloat(), getIntrinsicSize()).reduced(m.bezelWidth + 2.0f);
}

juce::Point<float> InvisConstellation::toPixels(juce::Point<float> n) const
{
    const auto a = getPadArea();
    return { a.getX() + n.x * a.getWidth(), a.getY() + n.y * a.getHeight() };
}

juce::Point<float> InvisConstellation::toNormalized(juce::Point<float> p) const
{
    const auto a = getPadArea();
    if (a.getWidth() <= 0.0f || a.getHeight() <= 0.0f) return { 0.5f, 0.5f };

    return { juce::jlimit(0.0f, 1.0f, (p.x - a.getX()) / a.getWidth()),
             juce::jlimit(0.0f, 1.0f, (p.y - a.getY()) / a.getHeight()) };
}

float InvisConstellation::radiusToPixels(float r) const
{
    return r * getPadArea().getWidth();
}

// --- Halo weights -----------------------------------------------------------------------------

std::vector<float> InvisConstellation::computeWeights(const std::vector<ConstellationNode>& nodes,
                                                      juce::Point<float> observerPosition)
{
    std::vector<float> weights;
    weights.reserve(nodes.size());

    for (const auto& n : nodes)
    {
        if (n.radius <= 1.0e-4f) { weights.push_back(0.0f); continue; }

        const float dist = observerPosition.getDistanceFrom(n.position);
        const float t = juce::jlimit(0.0f, 1.0f, dist / n.radius);

        // Smoothstep falloff: flat-topped at the star and flat-bottomed at the halo edge, so
        // neither has a hard corner you can hear as a step while dragging.
        const float s = t * t * (3.0f - 2.0f * t);
        weights.push_back(1.0f - s);
    }

    return weights;
}

// --- Figures, derived from the links ----------------------------------------------------------

namespace {

/** Everything reachable from `from`, optionally pretending one edge is not there. */
void reachFrom(const std::vector<std::vector<int>>& adj,
               int from,
               std::vector<bool>& reached,
               int skipA = -1, int skipB = -1)
{
    std::vector<int> stack { from };
    reached[static_cast<size_t>(from)] = true;

    while (!stack.empty())
    {
        const int cur = stack.back(); stack.pop_back();

        for (int nb : adj[static_cast<size_t>(cur)])
        {
            // Skipping one edge is how a bridge is tested: sever it and see if the far end is
            // still reachable by any other route.
            if ((cur == skipA && nb == skipB) || (cur == skipB && nb == skipA)) continue;
            if (reached[static_cast<size_t>(nb)]) continue;

            reached[static_cast<size_t>(nb)] = true;
            stack.push_back(nb);
        }
    }
}

/**
 * The colour of several stars shining together.
 *
 * NOT a per-channel average. Averaging red, green and blue gives literally grey - which is why
 * every closed constellation came out the same dead white no matter what was in it. Hue is an
 * ANGLE, so it has to be averaged as one: sum the member hues as unit vectors and take the
 * direction of the result. Saturation is then taken at its strongest rather than averaged, because
 * mixing two vivid lights should not produce a washed-out one.
 *
 * The length of that vector sum is the useful extra: 1 when every member agrees on a hue, near 0
 * when they sit opposite each other on the wheel. That is `spread`, and rather than fading the
 * colour out - the grey trap again - it is what earns a figure of clashing stars an IRIDESCENT
 * field instead of a flat one.
 */
struct StarLight
{
    juce::Colour colour;
    float spread { 0.0f };   // 0 = members agree on a hue, 1 = they are scattered around the wheel
};

StarLight mixStarLight(const std::vector<juce::Colour>& colours,
                       const std::vector<float>& weights)
{
    StarLight out { juce::Colours::white, 0.0f };
    if (colours.empty()) return out;

    float x = 0.0f, y = 0.0f, wSum = 0.0f;
    float sat = 0.0f, bri = 0.0f;

    for (size_t i = 0; i < colours.size(); ++i)
    {
        const float w = std::max(1.0e-4f, i < weights.size() ? weights[i] : 1.0f);

        // Weighted by saturation as well: a near-grey member should not get an equal vote on what
        // hue the instrument is.
        const float vote = w * (0.15f + 0.85f * colours[i].getSaturation());
        const float h = colours[i].getHue() * juce::MathConstants<float>::twoPi;

        x += std::cos(h) * vote;
        y += std::sin(h) * vote;
        wSum += vote;

        sat = std::max(sat, colours[i].getSaturation());
        bri = std::max(bri, colours[i].getBrightness());
    }

    if (wSum <= 1.0e-6f) return out;

    const float mag = std::sqrt(x * x + y * y) / wSum;
    float hue = std::atan2(y, x) / juce::MathConstants<float>::twoPi;
    if (hue < 0.0f) hue += 1.0f;

    out.spread = juce::jlimit(0.0f, 1.0f, 1.0f - mag);

    // Scattered members get a small saturation LIFT rather than a fade: the point of a mixed
    // constellation is that it looks like something you could not get from one star.
    out.colour = juce::Colour::fromHSV(hue,
                                       juce::jlimit(0.0f, 1.0f, sat * (1.0f + 0.20f * out.spread)),
                                       juce::jlimit(0.0f, 1.0f, bri),
                                       1.0f);
    return out;
}

/** The travelling charge itself: a bright core in a soft bloom, drawn at one point on a line. */
void drawSpark(juce::Graphics& g, juce::Point<float> at, juce::Colour tint, float size, float alpha)
{
    const float bloom = size * 3.2f;

    g.setGradientFill(juce::ColourGradient(tint.withAlpha(0.55f * alpha), at.x, at.y,
                                           juce::Colours::transparentBlack, at.x + bloom, at.y, true));
    g.fillEllipse(at.x - bloom, at.y - bloom, bloom * 2.0f, bloom * 2.0f);

    g.setColour(tint.brighter(0.6f).withAlpha(0.85f * alpha));
    g.fillEllipse(at.x - size, at.y - size, size * 2.0f, size * 2.0f);

    g.setColour(juce::Colours::white.withAlpha(0.9f * alpha));
    g.fillEllipse(at.x - size * 0.45f, at.y - size * 0.45f, size * 0.9f, size * 0.9f);
}

} // namespace

std::vector<ConstellationFigure> InvisConstellation::buildFigures(size_t numNodes,
                                                                  const std::vector<StarLink>& links)
{
    std::vector<ConstellationFigure> figures;
    if (numNodes == 0) return figures;

    // No socket cap: a star may carry as many lines as you draw. The shape of a component is then
    // whatever you made, and the routing rule reads off it rather than being enforced into it.
    std::vector<std::vector<int>> adj(numNodes);
    std::vector<std::pair<int, int>> edges;

    for (const auto& l : links)
    {
        if (l.a < 0 || l.b < 0 || l.a == l.b) continue;
        if (l.a >= static_cast<int>(numNodes) || l.b >= static_cast<int>(numNodes)) continue;

        adj[static_cast<size_t>(l.a)].push_back(l.b);
        adj[static_cast<size_t>(l.b)].push_back(l.a);
        edges.emplace_back(l.a, l.b);
    }

    // BRIDGES: an edge whose removal severs its own two ends. With a chart this size, testing each
    // edge by walking the graph without it is cheaper to read and to trust than a lowlink DFS, and
    // the cost is nothing.
    std::vector<bool> isBridge(edges.size(), false);

    for (size_t e = 0; e < edges.size(); ++e)
    {
        std::vector<bool> reached(numNodes, false);
        reachFrom(adj, edges[e].first, reached, edges[e].first, edges[e].second);
        isBridge[e] = !reached[static_cast<size_t>(edges[e].second)];
    }

    // Cutting every bridge leaves the 2-edge-connected pieces: exactly the cycles, each of which
    // is one parallel cluster. Everything else falls out as a cluster of one.
    std::vector<std::vector<int>> coreAdj(numNodes);
    for (size_t e = 0; e < edges.size(); ++e)
    {
        if (isBridge[e]) continue;
        coreAdj[static_cast<size_t>(edges[e].first)].push_back(edges[e].second);
        coreAdj[static_cast<size_t>(edges[e].second)].push_back(edges[e].first);
    }

    std::vector<int> clusterOf(numNodes, -1);
    std::vector<ConstellationCluster> clusters;

    for (size_t i = 0; i < numNodes; ++i)
    {
        if (clusterOf[i] >= 0) continue;

        std::vector<bool> reached(numNodes, false);
        reachFrom(coreAdj, static_cast<int>(i), reached);

        ConstellationCluster cl;
        for (size_t k = 0; k < numNodes; ++k)
            if (reached[k]) { clusterOf[k] = static_cast<int>(clusters.size()); cl.stars.push_back(static_cast<int>(k)); }

        cl.parallel = cl.stars.size() > 1;
        clusters.push_back(std::move(cl));
    }

    // Now the components themselves, and which of the clusters each one owns.
    std::vector<bool> seen(numNodes, false);

    for (size_t start = 0; start < numNodes; ++start)
    {
        if (seen[start]) continue;

        std::vector<bool> reached(numNodes, false);
        reachFrom(adj, static_cast<int>(start), reached);

        ConstellationFigure fig;
        std::vector<int> localOf(clusters.size(), -1);

        for (size_t k = 0; k < numNodes; ++k)
        {
            if (!reached[k]) continue;

            seen[k] = true;
            fig.stars.push_back(static_cast<int>(k));

            const int c = clusterOf[k];
            if (localOf[static_cast<size_t>(c)] < 0)
            {
                localOf[static_cast<size_t>(c)] = static_cast<int>(fig.clusters.size());
                fig.clusters.push_back(clusters[static_cast<size_t>(c)]);
            }
        }

        // Every bridge inside this component is a serial hop between two of its clusters.
        for (size_t e = 0; e < edges.size(); ++e)
        {
            if (!isBridge[e]) continue;
            if (!reached[static_cast<size_t>(edges[e].first)]) continue;

            const int ca = localOf[static_cast<size_t>(clusterOf[static_cast<size_t>(edges[e].first)])];
            const int cb = localOf[static_cast<size_t>(clusterOf[static_cast<size_t>(edges[e].second)])];
            if (ca >= 0 && cb >= 0 && ca != cb) fig.clusterLinks.emplace_back(ca, cb);
        }

        if (!fig.stars.empty()) figures.push_back(std::move(fig));
    }

    return figures;
}

std::vector<ConstellationCluster> InvisConstellation::getParallelClusters() const
{
    std::vector<ConstellationCluster> out;

    for (const auto& fig : buildFigures(nodes.size(), links))
        for (const auto& cl : fig.clusters)
            if (cl.parallel) out.push_back(cl);

    return out;
}

// --- Figure geometry --------------------------------------------------------------------------

juce::Point<float> InvisConstellation::figureCentroid(const std::vector<ConstellationNode>& nodes,
                                                      const std::vector<int>& stars)
{
    juce::Point<float> c;
    int n = 0;

    for (int idx : stars)
    {
        if (idx < 0 || idx >= static_cast<int>(nodes.size())) continue;
        c += nodes[static_cast<size_t>(idx)].position;
        ++n;
    }

    return n > 0 ? c / static_cast<float>(n) : juce::Point<float>(0.5f, 0.5f);
}

float InvisConstellation::figureRadius(const std::vector<ConstellationNode>& nodes,
                                       const std::vector<int>& stars)
{
    float sum = 0.0f;
    int n = 0;

    for (int idx : stars)
    {
        if (idx < 0 || idx >= static_cast<int>(nodes.size())) continue;
        sum += nodes[static_cast<size_t>(idx)].radius;
        ++n;
    }

    return n > 0 ? sum / static_cast<float>(n) : 0.26f;
}

juce::Path InvisConstellation::clusterRegion(const std::vector<ConstellationNode>& nodes,
                                             const std::vector<StarLink>& links,
                                             const std::vector<int>& stars)
{
    juce::Path region;
    if (stars.size() < 3) return region;

    const auto member = [&stars](int idx)
    { return std::find(stars.begin(), stars.end(), idx) != stars.end(); };

    // Only the lines that live entirely inside this cluster bound anything it encloses.
    std::vector<std::pair<int, int>> edges;
    for (const auto& l : links)
    {
        if (l.a < 0 || l.b < 0 || l.a == l.b) continue;
        if (l.a >= static_cast<int>(nodes.size()) || l.b >= static_cast<int>(nodes.size())) continue;
        if (member(l.a) && member(l.b)) edges.emplace_back(l.a, l.b);
    }

    if (edges.size() < 3) return region;

    std::vector<std::vector<int>> adj(nodes.size());
    for (const auto& e : edges)
    {
        adj[static_cast<size_t>(e.first)].push_back(e.second);
        adj[static_cast<size_t>(e.second)].push_back(e.first);
    }

    // A spanning tree, so every remaining edge closes exactly one cycle. Depth is kept alongside
    // the parent to walk the two ends up to their meeting point without a second search.
    const int root = stars.front();
    std::vector<int> parent(nodes.size(), -2), depth(nodes.size(), 0);
    std::vector<int> queue { root };
    parent[static_cast<size_t>(root)] = -1;

    for (size_t head = 0; head < queue.size(); ++head)
    {
        const int cur = queue[head];
        for (int nb : adj[static_cast<size_t>(cur)])
            if (parent[static_cast<size_t>(nb)] == -2)
            {
                parent[static_cast<size_t>(nb)] = cur;
                depth[static_cast<size_t>(nb)] = depth[static_cast<size_t>(cur)] + 1;
                queue.push_back(nb);
            }
    }

    const auto isTreeEdge = [&parent](int a, int b)
    { return parent[static_cast<size_t>(a)] == b || parent[static_cast<size_t>(b)] == a; };

    for (const auto& e : edges)
    {
        if (isTreeEdge(e.first, e.second)) continue;
        if (parent[static_cast<size_t>(e.first)] == -2) continue;   // not reached: not our cluster

        // The fundamental cycle this edge closes: both ends climb to their common ancestor.
        std::vector<int> up, down;
        int a = e.first, b = e.second;

        while (depth[static_cast<size_t>(a)] > depth[static_cast<size_t>(b)])
        { up.push_back(a); a = parent[static_cast<size_t>(a)]; }
        while (depth[static_cast<size_t>(b)] > depth[static_cast<size_t>(a)])
        { down.push_back(b); b = parent[static_cast<size_t>(b)]; }

        while (a != b)
        {
            up.push_back(a);   a = parent[static_cast<size_t>(a)];
            down.push_back(b); b = parent[static_cast<size_t>(b)];
            if (a < 0 || b < 0) break;
        }

        if (a < 0) continue;

        std::vector<int> cycle = up;
        cycle.push_back(a);
        cycle.insert(cycle.end(), down.rbegin(), down.rend());

        if (cycle.size() < 3) continue;

        // Every cycle is wound the SAME way before it goes in. Non-zero winding then unions the
        // areas; mixed windings would punch the overlap out as a hole instead.
        float twiceArea = 0.0f;
        for (size_t i = 0; i < cycle.size(); ++i)
        {
            const auto p0 = nodes[static_cast<size_t>(cycle[i])].position;
            const auto p1 = nodes[static_cast<size_t>(cycle[(i + 1) % cycle.size()])].position;
            twiceArea += p0.x * p1.y - p1.x * p0.y;
        }

        if (twiceArea < 0.0f) std::reverse(cycle.begin(), cycle.end());

        region.startNewSubPath(nodes[static_cast<size_t>(cycle.front())].position);
        for (size_t i = 1; i < cycle.size(); ++i)
            region.lineTo(nodes[static_cast<size_t>(cycle[i])].position);
        region.closeSubPath();
    }

    return region;
}

float InvisConstellation::distanceToFigure(const std::vector<ConstellationNode>& nodes,
                                           const std::vector<StarLink>& links,
                                           const std::vector<int>& stars,
                                           juce::Point<float> p)
{
    if (stars.empty()) return 1.0f;

    if (stars.size() == 1)
    {
        const int idx = stars[0];
        if (idx < 0 || idx >= static_cast<int>(nodes.size())) return 1.0f;
        return p.getDistanceFrom(nodes[static_cast<size_t>(idx)].position);
    }

    // Inside what the cycles enclose counts as zero distance: the region IS the instrument, so
    // standing anywhere within it means standing in it - not "nearer to one vertex than another".
    const auto region = clusterRegion(nodes, links, stars);
    if (!region.isEmpty() && region.contains(p)) return 0.0f;

    // Otherwise: out to the nearest line the cluster actually owns. Measuring to hull edges
    // instead would report a distance to a boundary that was never drawn.
    float best = std::numeric_limits<float>::max();

    const auto member = [&stars](int idx)
    { return std::find(stars.begin(), stars.end(), idx) != stars.end(); };

    for (const auto& l : links)
    {
        if (l.a < 0 || l.b < 0) continue;
        if (l.a >= static_cast<int>(nodes.size()) || l.b >= static_cast<int>(nodes.size())) continue;
        if (!member(l.a) || !member(l.b)) continue;

        const auto a = nodes[static_cast<size_t>(l.a)].position;
        const auto b = nodes[static_cast<size_t>(l.b)].position;

        const auto ab = b - a;
        const float lenSq = ab.x * ab.x + ab.y * ab.y;

        float t = lenSq > 1.0e-9f ? ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / lenSq : 0.0f;
        t = juce::jlimit(0.0f, 1.0f, t);

        best = std::min(best, p.getDistanceFrom({ a.x + ab.x * t, a.y + ab.y * t }));
    }

    // No lines at all (a lone star dressed as a cluster): fall back to the nearest member.
    if (best == std::numeric_limits<float>::max())
        for (int idx : stars)
            if (idx >= 0 && idx < static_cast<int>(nodes.size()))
                best = std::min(best, p.getDistanceFrom(nodes[static_cast<size_t>(idx)].position));

    return best;
}

float InvisConstellation::getFigureSensitivity(const std::vector<int>& stars) const
{
    for (int idx : stars)
        if (idx >= 0 && idx < static_cast<int>(nodes.size()))
            return nodes[static_cast<size_t>(idx)].sensitivity;

    return 1.0f;
}

void InvisConstellation::setFigureSensitivity(int anyMemberIndex, float bipolar)
{
    // The shared control belongs to the CLUSTER, not the component: a triangle with a star hung
    // off it has one hub for the triangle, and the hanging star keeps its own sensitivity.
    for (const auto& cl : getParallelClusters())
    {
        if (std::find(cl.stars.begin(), cl.stars.end(), anyMemberIndex) == cl.stars.end())
            continue;

        const float v = juce::jlimit(-1.0f, 1.0f, bipolar);
        for (int idx : cl.stars)
            if (idx >= 0 && idx < static_cast<int>(nodes.size()))
                nodes[static_cast<size_t>(idx)].sensitivity = v;

        break;
    }

    if (onGeometryChanged) onGeometryChanged();
    notifyWeights();
    repaint();
}

// --- Links ------------------------------------------------------------------------------------

int InvisConstellation::countLinks(int starIndex) const
{
    int n = 0;
    for (const auto& l : links) if (l.touches(starIndex)) ++n;
    return n;
}

bool InvisConstellation::canLink(int a, int b) const
{
    if (a < 0 || b < 0 || a == b) return false;
    if (a >= static_cast<int>(nodes.size()) || b >= static_cast<int>(nodes.size())) return false;

    // Only the duplicate is refused. Branching is allowed: what a component MEANS is read off
    // its shape afterwards, so there is nothing to protect here.
    for (const auto& l : links) if (l.same(a, b)) return false;

    return true;
}

bool InvisConstellation::linkStars(int a, int b)
{
    if (!canLink(a, b)) return false;

    links.push_back({ a, b });
    if (onGeometryChanged) onGeometryChanged();
    notifyWeights();
    repaint();
    return true;
}

void InvisConstellation::unlinkStars(int a, int b)
{
    const auto before = links.size();
    links.erase(std::remove_if(links.begin(), links.end(),
                               [a, b](const StarLink& l) { return l.same(a, b); }),
                links.end());

    if (links.size() == before) return;

    if (onGeometryChanged) onGeometryChanged();
    notifyWeights();
    repaint();
}

// --- Routing ----------------------------------------------------------------------------------

std::vector<StarContribution> InvisConstellation::evaluate(const std::vector<ConstellationNode>& nodes,
                                                           const std::vector<StarLink>& links,
                                                           juce::Point<float> observerPosition,
                                                           int previousEntry)
{
    std::vector<StarContribution> out(nodes.size());

    const auto halo = computeWeights(nodes, observerPosition);
    const auto figures = buildFigures(nodes.size(), links);

    for (const auto& fig : figures)
    {
        if (fig.clusters.empty()) continue;

        // ENTRY. The signal walks in at the star nearest the observer - and therefore into the
        // CLUSTER that star belongs to. In a cycle that means the whole cycle is the entry stage,
        // which is what "a closed figure behaves as one node" has to mean at the door as well.
        int entryStar = fig.stars.front();
        float bestDist = std::numeric_limits<float>::max();

        for (int idx : fig.stars)
        {
            if (idx < 0 || idx >= static_cast<int>(nodes.size())) continue;

            float d = observerPosition.getDistanceFrom(nodes[static_cast<size_t>(idx)].position);

            // HYSTERESIS. On the midline between two stars the entry would otherwise flip - and
            // with it the whole processing order - on the slightest movement. The incumbent keeps
            // a 12% advantage, so a rival has to be decisively closer to take over.
            if (idx == previousEntry) d *= 0.88f;

            if (d < bestDist) { bestDist = d; entryStar = idx; }
        }

        int entryCluster = 0;
        for (size_t c = 0; c < fig.clusters.size(); ++c)
        {
            const auto& st = fig.clusters[c].stars;
            if (std::find(st.begin(), st.end(), entryStar) != st.end())
            {
                entryCluster = static_cast<int>(c);
                break;
            }
        }

        // HOW MUCH GETS IN. The observer's distance decides how much signal enters the figure at
        // all - a chain is not a free full send just because it exists. Only the hops BETWEEN
        // clusters are full.
        float arrival;

        if (fig.clusters[static_cast<size_t>(entryCluster)].parallel)
        {
            // For a cycle the door is the ENCLOSURE, not any one member: standing inside the
            // region means fully inside the instrument, which is what makes closing it mean
            // something.
            const auto& st = fig.clusters[static_cast<size_t>(entryCluster)].stars;
            const float shared = figureRadius(nodes, st);
            const float d = distanceToFigure(nodes, links, st, observerPosition);
            const float t = juce::jlimit(0.0f, 1.0f, shared > 1.0e-5f ? d / shared : 1.0f);

            arrival = 1.0f - (t * t * (3.0f - 2.0f * t));
        }
        else
        {
            arrival = halo[static_cast<size_t>(entryStar)];
        }

        const float entrySens = nodes[static_cast<size_t>(entryStar)].sensitivity;

        // SILENT IS NOT SHAPELESS. A figure the observer has walked away from used to be wiped
        // outright, chain order and all - so the chart forgot how it was wired the moment it went
        // quiet, and the routing animation had nothing left to draw. Level and structure are
        // different facts: the levels go to zero, the stages stay.
        const bool silent = std::abs(arrival * entrySens) <= 0.0005f;

        // Walk the CLUSTER TREE breadth-first from the entry. Contracting the cycles guarantees
        // this is a tree, so a figure of any shape - branches, loops, loops with branches - comes
        // out as an ordered run of stages with no special cases.
        std::vector<std::vector<int>> tree(fig.clusters.size());
        for (const auto& e : fig.clusterLinks)
        {
            tree[static_cast<size_t>(e.first)].push_back(e.second);
            tree[static_cast<size_t>(e.second)].push_back(e.first);
        }

        std::vector<bool> reached(fig.clusters.size(), false);
        std::vector<int> queue { entryCluster };
        reached[static_cast<size_t>(entryCluster)] = true;

        // Only a multi-stage figure has an entry worth marking: a lone star or a bare cycle has
        // nowhere else for the signal to go, so a gate ring there would be noise.
        const bool marksEntry = fig.clusters.size() > 1;

        int order = 0;
        for (size_t head = 0; head < queue.size(); ++head)
        {
            const int ci = queue[head];
            const auto& cl = fig.clusters[static_cast<size_t>(ci)];
            const bool isEntry = (ci == entryCluster);
            const float feed = isEntry ? arrival : 1.0f;   // hops between stages are 100%

            if (cl.parallel)
            {
                // Internal balance from each member's own proximity, so moving about inside a
                // cycle MORPHS between its members at a roughly steady total.
                float sum = 0.0f;
                for (int idx : cl.stars)
                    if (idx >= 0 && idx < static_cast<int>(nodes.size()))
                        sum += halo[static_cast<size_t>(idx)];

                // Dead centre of a big ring can sit outside every individual halo while still
                // being inside the figure. An equal share keeps the instrument audible there
                // instead of punching a silent hole through its middle.
                const bool useEqualShares = (sum <= 1.0e-5f);
                if (useEqualShares) sum = static_cast<float>(cl.stars.size());

                const float count = static_cast<float>(cl.stars.size());

                for (int idx : cl.stars)
                {
                    if (idx < 0 || idx >= static_cast<int>(nodes.size())) continue;

                    const float share = useEqualShares ? (1.0f / sum)
                                                       : (halo[static_cast<size_t>(idx)] / sum);

                    auto& c = out[static_cast<size_t>(idx)];
                    c.chainOrder = order;
                    c.isEntry = marksEntry && isEntry;
                    c.amount = silent ? 0.0f
                                      : feed * share * nodes[static_cast<size_t>(idx)].sensitivity;

                    // GLOW IS NOT THE SHARE. The share is normalised across the members, so a
                    // four-star cycle gives each one a quarter - and walking INTO the figure made
                    // everything dim instead of lighting up, which is exactly backwards. What the
                    // eye is being told is how strongly the stage is FED, so glow follows the
                    // arrival; the share only decides which member leads.
                    const float lead = juce::jlimit(0.0f, 1.0f, share * count);
                    c.glow = silent ? 0.0f : feed * (0.55f + 0.45f * lead);
                }
            }
            else
            {
                for (int idx : cl.stars)
                {
                    if (idx < 0 || idx >= static_cast<int>(nodes.size())) continue;

                    auto& c = out[static_cast<size_t>(idx)];
                    c.chainOrder = order;
                    c.isEntry = marksEntry && isEntry;
                    c.amount = silent ? 0.0f : feed * nodes[static_cast<size_t>(idx)].sensitivity;
                    c.glow = std::abs(c.amount);
                }
            }

            ++order;

            for (int nb : tree[static_cast<size_t>(ci)])
                if (!reached[static_cast<size_t>(nb)])
                {
                    reached[static_cast<size_t>(nb)] = true;
                    queue.push_back(nb);
                }
        }

        // The tree is connected, so every stage is reached - but a member that somehow fell off
        // the walk must still read as silent rather than keeping stale state.
        for (size_t c = 0; c < fig.clusters.size(); ++c)
        {
            if (reached[c]) continue;

            for (int idx : fig.clusters[c].stars)
                if (idx >= 0 && idx < static_cast<int>(nodes.size()))
                    out[static_cast<size_t>(idx)] = {};
        }
    }

    return out;
}

ChartFrame InvisConstellation::takeSnapshot() const
{
    ChartFrame frame;

    for (int i = 0; i < getNumObservers(); ++i)
        frame.heard[static_cast<size_t>(i)] = getContributions(i);

    // An unused observer still needs a row: everything downstream indexes by observer, and an
    // empty vector there would be a bounds check waiting to be forgotten.
    for (int i = getNumObservers(); i < 2; ++i)
        frame.heard[static_cast<size_t>(i)].assign(nodes.size(), {});

    frame.figures = buildFigures(nodes.size(), links);

    for (const auto& fig : frame.figures)
        for (const auto& cl : fig.clusters)
            if (cl.parallel) frame.parallel.push_back(cl);

    // How many stages each star's figure has, so the pulse knows how long its round trip is.
    frame.stages.assign(nodes.size(), 0);

    for (const auto& fig : frame.figures)
    {
        int last = -1;
        for (int idx : fig.stars)
            if (idx >= 0 && idx < static_cast<int>(nodes.size()))
                last = std::max(last, frame.heard[0][static_cast<size_t>(idx)].chainOrder);

        for (int idx : fig.stars)
            if (idx >= 0 && idx < static_cast<int>(nodes.size()))
                frame.stages[static_cast<size_t>(idx)] = last + 1;
    }

    return frame;
}

std::vector<StarContribution> InvisConstellation::getContributions(int observerIndex) const
{
    const int i = juce::jlimit(0, 1, observerIndex);
    auto result = evaluate(nodes, links, observers[static_cast<size_t>(i)], lastEntry[i]);

    lastEntry[i] = -1;
    for (size_t k = 0; k < result.size(); ++k)
        if (result[k].isEntry) { lastEntry[i] = static_cast<int>(k); break; }

    return result;
}

void InvisConstellation::notifyWeights()
{
    if (onWeightsChanged == nullptr) return;

    for (int i = 0; i < getNumObservers(); ++i)
        onWeightsChanged(i, getWeights(i));
}

// --- Channel mode -----------------------------------------------------------------------------

void InvisConstellation::setChannelMode(ConstellationChannelMode mode)
{
    if (channelMode == mode) return;

    channelMode = mode;

    // Splitting one observer into two, or collapsing two into one, must not be audible on its
    // own: both start where the single one stood. Collapsing keeps observer A rather than an
    // average, because an average is a position the user never chose and never heard.
    observers[1] = observers[0];

    notifyWeights();
    repaint();
}

juce::Colour InvisConstellation::getObserverColour(int observerIndex) const
{
    const auto theme = getEffectiveTheme();

    // The second observer is tinted so its beams can be told apart at a glance - with two of them
    // every star grows two tethers, and undifferentiated ones would be unreadable.
    if (observerIndex == 1)
        return theme.accentSecondary.isTransparent() ? juce::Colour::fromRGB(255, 171, 0)
                                                     : theme.accentSecondary;

    return theme.textPrimary;
}

// --- Stars ------------------------------------------------------------------------------------

int InvisConstellation::addNode(const juce::String& label, juce::Colour colour)
{
    if (static_cast<int>(nodes.size()) >= kMaxNodes) return -1;

    // New stars land on a ring around the centre so they never stack on each other or on the
    // observer, which would make the newest one impossible to grab.
    const int n = static_cast<int>(nodes.size());
    const float angle = juce::MathConstants<float>::twoPi * static_cast<float>(n) / 6.0f
                      - juce::MathConstants<float>::halfPi;
    const float ring = 0.33f;

    ConstellationNode node;
    node.position = { 0.5f + std::cos(angle) * ring, 0.5f + std::sin(angle) * ring };
    node.radius = 0.26f;
    node.colour = colour;
    node.label = label;

    nodes.push_back(node);

    // Arrives UNLINKED. A lone star is simply a parallel send, which is the least surprising
    // default: adding one never silently rewires a figure you already drew.

    if (onGeometryChanged) onGeometryChanged();
    notifyWeights();
    repaint();

    return n;
}

void InvisConstellation::removeNode(int index)
{
    if (index < 0 || index >= static_cast<int>(nodes.size())) return;

    nodes.erase(nodes.begin() + index);

    // Drop every line touching it, then repair the rest - all their indices shifted down.
    links.erase(std::remove_if(links.begin(), links.end(),
                               [index](const StarLink& l) { return l.touches(index); }),
                links.end());

    for (auto& l : links)
    {
        if (l.a > index) --l.a;
        if (l.b > index) --l.b;
    }

    hoveredNode = -1;
    hoveredLink = -1;

    if (onGeometryChanged) onGeometryChanged();
    notifyWeights();
    repaint();
}

void InvisConstellation::clearNodes()
{
    nodes.clear();
    links.clear();
    hoveredNode = -1;
    hoveredLink = -1;

    if (onGeometryChanged) onGeometryChanged();
    notifyWeights();
    repaint();
}

void InvisConstellation::randomise()
{
    const int n = static_cast<int>(nodes.size());
    if (n == 0) return;

    // Jittered angular sweep: each star gets its own slice of the circle plus noise. Uniform
    // random placement clusters badly and would routinely stack stars on top of each other.
    const float slice = juce::MathConstants<float>::twoPi / static_cast<float>(n);

    for (int i = 0; i < n; ++i)
    {
        const float angle = slice * static_cast<float>(i) + rng.nextFloat() * slice * 0.7f;
        const float dist = 0.18f + rng.nextFloat() * 0.26f;

        nodes[static_cast<size_t>(i)].position = { juce::jlimit(0.06f, 0.94f, 0.5f + std::cos(angle) * dist),
                                                   juce::jlimit(0.06f, 0.94f, 0.5f + std::sin(angle) * dist) };
        nodes[static_cast<size_t>(i)].radius = juce::jlimit(kMinRadius, kMaxRadius,
                                                            0.18f + rng.nextFloat() * 0.22f);
    }

    if (onGeometryChanged) onGeometryChanged();
    notifyWeights();
    repaint();
}

void InvisConstellation::setNodePosition(int index, juce::Point<float> n)
{
    if (index < 0 || index >= static_cast<int>(nodes.size())) return;

    nodes[static_cast<size_t>(index)].position = { juce::jlimit(0.0f, 1.0f, n.x),
                                                   juce::jlimit(0.0f, 1.0f, n.y) };
    if (onGeometryChanged) onGeometryChanged();
    notifyWeights();
    repaint();
}

void InvisConstellation::setNodeRadius(int index, float r)
{
    if (index < 0 || index >= static_cast<int>(nodes.size())) return;

    nodes[static_cast<size_t>(index)].radius = juce::jlimit(kMinRadius, kMaxRadius, r);
    if (onGeometryChanged) onGeometryChanged();
    notifyWeights();
    repaint();
}

void InvisConstellation::setNodeSensitivity(int index, float bipolar)
{
    if (index < 0 || index >= static_cast<int>(nodes.size())) return;

    nodes[static_cast<size_t>(index)].sensitivity = juce::jlimit(-1.0f, 1.0f, bipolar);
    if (onGeometryChanged) onGeometryChanged();
    notifyWeights();
    repaint();
}

void InvisConstellation::setNodeLabel(int index, const juce::String& label)
{
    if (index < 0 || index >= static_cast<int>(nodes.size())) return;
    nodes[static_cast<size_t>(index)].label = label;
    repaint();
}

void InvisConstellation::setNodeColour(int index, juce::Colour colour)
{
    if (index < 0 || index >= static_cast<int>(nodes.size())) return;
    nodes[static_cast<size_t>(index)].colour = colour;
    repaint();
}

void InvisConstellation::setObserverPosition(int observerIndex, juce::Point<float> n)
{
    const int i = juce::jlimit(0, 1, observerIndex);
    observers[static_cast<size_t>(i)] = { juce::jlimit(0.0f, 1.0f, n.x), juce::jlimit(0.0f, 1.0f, n.y) };

    // In a linked mode the two are the same listener and must not drift apart
    if (!constellationModeHasTwoObservers(channelMode))
        observers[1] = observers[0];

    notifyWeights();
    repaint();
}

void InvisConstellation::insertNodeOnLink(int linkIndex, juce::Point<float> normalized)
{
    if (static_cast<int>(nodes.size()) >= kMaxNodes) return;
    if (linkIndex < 0 || linkIndex >= static_cast<int>(links.size())) return;

    const int a = links[static_cast<size_t>(linkIndex)].a;
    const int b = links[static_cast<size_t>(linkIndex)].b;
    if (a < 0 || b < 0) return;

    ConstellationNode node;
    node.position = normalized;
    node.radius = (nodes[static_cast<size_t>(a)].radius + nodes[static_cast<size_t>(b)].radius) * 0.5f;
    node.colour = nodes[static_cast<size_t>(a)].colour
                    .interpolatedWith(nodes[static_cast<size_t>(b)].colour, 0.5f);
    node.label = "NEW";

    const int newIndex = static_cast<int>(nodes.size());
    nodes.push_back(node);

    // SPLIT the line rather than adding beside it: clicking a line means "put a stage here", and
    // the stage has to end up wired in series where the line used to run.
    links.erase(links.begin() + linkIndex);
    links.push_back({ a, newIndex });
    links.push_back({ newIndex, b });

    hoveredLink = -1;

    if (onNodeInserted) onNodeInserted(newIndex);
    if (onGeometryChanged) onGeometryChanged();
    notifyWeights();
    repaint();
}

void InvisConstellation::tickAnimation(float dt)
{
    // THE CHART IS ALWAYS RUNNING. This used to stop the clock whenever nothing was loud enough,
    // which meant the one thing the animation exists to show - how the chains you built are wired,
    // and which way they run - was only visible while you happened to be standing close to them.
    // What a chain DOES is true whether or not you are listening to it.
    flowPhase += dt;
    if (flowPhase > 1000.0f) flowPhase -= 1000.0f;

    pulseClock += static_cast<double>(dt);
    if (pulseClock > 86400.0) pulseClock -= 86400.0;

    repaint();
}

float InvisConstellation::pulseOnSegment(int stages, int order) const
{
    if (stages <= 0 || order < 0 || order >= stages) return -1.0f;

    const double cycle = static_cast<double>(stages) * kHopSeconds;
    const double local = std::fmod(pulseClock, cycle);
    const int segment = static_cast<int>(local / kHopSeconds);

    if (segment != order) return -1.0f;

    return static_cast<float>((local - segment * kHopSeconds) / kHopSeconds);
}

float InvisConstellation::stageFlash(int stages, int order) const
{
    if (stages <= 0 || order < 0 || order >= stages) return 0.0f;

    // A stage is struck at the END of the segment that feeds it: segment 0 carries the signal from
    // the observer, so stage 0 lights one hop in.
    const double cycle = static_cast<double>(stages) * kHopSeconds;
    const double local = std::fmod(pulseClock, cycle);

    double age = local - (order + 1) * static_cast<double>(kHopSeconds);
    while (age < 0.0) age += cycle;

    return static_cast<float>(std::exp(-age / kFlashDecay));
}

void InvisConstellation::resized()
{
    const auto intrinsic = getIntrinsicSize();
    jassert(getWidth() == 0 || (getWidth() == intrinsic.x && getHeight() == intrinsic.y));
}

// --- Hit testing ------------------------------------------------------------------------------

int InvisConstellation::hitTestNodeCore(juce::Point<float> p) const
{
    const auto m = getMetrics(padSize);

    // Reverse order: the last-drawn star sits on top, so it must be the first one grabbed.
    for (int i = static_cast<int>(nodes.size()) - 1; i >= 0; --i)
        if (toPixels(nodes[static_cast<size_t>(i)].position).getDistanceFrom(p) <= m.nodeRadius + 3.0f)
            return i;

    return -1;
}

int InvisConstellation::hitTestRim(juce::Point<float> p) const
{
    const auto m = getMetrics(padSize);
    const float inner = m.nodeRadius + 3.0f;
    const float outer = m.nodeRadius + 11.0f;

    for (int i = static_cast<int>(nodes.size()) - 1; i >= 0; --i)
    {
        const float d = toPixels(nodes[static_cast<size_t>(i)].position).getDistanceFrom(p);
        if (d > inner && d <= outer) return i;
    }

    return -1;
}

int InvisConstellation::hitTestHub(juce::Point<float> p) const
{
    const float tol = getMetrics(padSize).nodeRadius * 0.9f + 7.0f;

    for (const auto& cl : getParallelClusters())
    {
        if (cl.stars.size() < 3) continue;

        if (toPixels(figureCentroid(nodes, cl.stars)).getDistanceFrom(p) <= tol)
            return cl.stars.front();   // any member addresses the cluster
    }

    return -1;
}

int InvisConstellation::hitTestObserver(juce::Point<float> p) const
{
    const float tol = getMetrics(padSize).observerRadius + 6.0f;

    // Reverse order so a stacked pair can be pulled apart: grabbing takes B first, leaving A.
    for (int i = getNumObservers() - 1; i >= 0; --i)
        if (toPixels(observers[static_cast<size_t>(i)]).getDistanceFrom(p) <= tol)
            return i;

    return -1;
}

int InvisConstellation::hitTestLink(juce::Point<float> p, juce::Point<float>& pointOnLink) const
{
    const float tolerance = 7.0f;
    int best = -1;
    float bestDist = tolerance;

    for (size_t i = 0; i < links.size(); ++i)
    {
        const auto& l = links[i];
        if (l.a < 0 || l.b < 0) continue;
        if (l.a >= static_cast<int>(nodes.size()) || l.b >= static_cast<int>(nodes.size())) continue;

        const auto a = toPixels(nodes[static_cast<size_t>(l.a)].position);
        const auto b = toPixels(nodes[static_cast<size_t>(l.b)].position);

        const auto ab = b - a;
        const float lenSq = ab.x * ab.x + ab.y * ab.y;
        if (lenSq < 1.0f) continue;

        // Clamp well clear of the endpoints so the line never competes with the stars on it.
        float t = ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / lenSq;
        if (t < 0.15f || t > 0.85f) continue;

        const juce::Point<float> proj { a.x + ab.x * t, a.y + ab.y * t };
        const float d = proj.getDistanceFrom(p);

        if (d < bestDist) { bestDist = d; best = static_cast<int>(i); pointOnLink = proj; }
    }

    return best;
}

// --- Interaction ------------------------------------------------------------------------------

void InvisConstellation::mouseDown(const juce::MouseEvent& e)
{
    dragMoved = false;

    // A LINE IN FLIGHT OWNS THE NEXT CLICK. Nothing else may claim it - not the observer, not a
    // star's core - because while a line is armed the only question on screen is where it lands.
    if (linkFrom >= 0)
    {
        int target = hitTestNodeCore(e.position);
        if (target < 0) target = hitTestRim(e.position);

        if (target >= 0 && canLink(linkFrom, target)) linkStars(linkFrom, target);

        linkFrom = -1;      // a click on bare sky simply throws the line away
        linkTarget = -1;
        grab = Grab::None;
        repaint();
        return;
    }

    // The observer wins the priority order: it is small, it is what you reach for most, and it
    // must never be blocked by something drawn under it.
    if (const int o = hitTestObserver(e.position); o >= 0)
    {
        grab = Grab::Observer;
        grabbedObserver = o;
        return;
    }

    // The break handle is a tiny target sitting ON a line, so it has to be tested before the line.
    if (overBreakHandle && hoveredLink >= 0 && hoveredLink < static_cast<int>(links.size()))
    {
        const auto l = links[static_cast<size_t>(hoveredLink)];
        unlinkStars(l.a, l.b);
        hoveredLink = -1;
        overBreakHandle = false;
        return;
    }

    // The hub sits inside the enclosure, where nothing else competes for the same pixels, so it
    // is tested early - but after the observer, which may be parked on top of it.
    if (const int hub = hitTestHub(e.position); hub >= 0)
    {
        grab = Grab::Hub;
        grabbedIndex = hub;
        grabStartRadius = nodes[static_cast<size_t>(hub)].sensitivity;
        grabStartPos = e.position;
        repaint();
        return;
    }

    if (const int core = hitTestNodeCore(e.position); core >= 0)
    {
        grabbedIndex = core;

        // Same press, two jobs, so the second one takes a modifier.
        if (e.mods.isAltDown() || e.mods.isRightButtonDown())
        {
            grab = Grab::Sensitivity;
            grabStartRadius = nodes[static_cast<size_t>(core)].sensitivity;
            grabStartPos = e.position;
        }
        else
        {
            grab = Grab::Node;
        }

        repaint();
        return;
    }

    // The RIM is the socket collar. Clicking it ARMS a line: it then follows the cursor with the
    // button up until you click a star to land it. Dragging still works and lands on release, so
    // whichever gesture you reach for first is the right one.
    if (const int rim = hitTestRim(e.position); rim >= 0)
    {
        grab = Grab::Linking;
        linkFrom = rim;
        linkCursor = e.position;
        linkTarget = -1;
        repaint();
        return;
    }

    // On a line: materialise the star that has been previewing there.
    juce::Point<float> onLink;
    if (const int link = hitTestLink(e.position, onLink); link >= 0)
    {
        insertNodeOnLink(link, toNormalized(onLink));
        return;
    }

    // Bare sky does NOTHING. Where the observer stands is a deliberate act - teleporting it on a
    // stray click loses the position you were auditioning.
    grab = Grab::None;
}

void InvisConstellation::mouseDrag(const juce::MouseEvent& e)
{
    dragMoved = true;

    switch (grab)
    {
        case Grab::Observer:
            setObserverPosition(grabbedObserver, toNormalized(e.position));
            break;

        case Grab::Node:
            setNodePosition(grabbedIndex, toNormalized(e.position));
            break;

        case Grab::Hub:
        {
            // Same knob gesture as a single star, so the shared control feels like the ones it
            // replaces rather than like a different kind of thing.
            const float travel = grabStartPos.y - e.position.y;
            const float scale = e.mods.isShiftDown() ? 0.0012f : 0.006f;
            setFigureSensitivity(grabbedIndex, grabStartRadius + travel * scale);
            break;
        }

        case Grab::Sensitivity:
        {
            // Knob behaviour: vertical travel, referenced to the value at press so the gesture
            // cannot drift, and shift for a fine pass.
            const float travel = grabStartPos.y - e.position.y;
            const float scale = e.mods.isShiftDown() ? 0.0012f : 0.006f;
            setNodeSensitivity(grabbedIndex, grabStartRadius + travel * scale);
            break;
        }

        case Grab::Linking:
        {
            linkCursor = e.position;

            const int over = hitTestNodeCore(e.position);
            linkTarget = (over >= 0 && canLink(linkFrom, over)) ? over : -1;
            repaint();
            break;
        }

        case Grab::None:
        default:
            break;
    }
}

void InvisConstellation::mouseUp(const juce::MouseEvent&)
{
    if (grab == Grab::Linking)
    {
        // Released after dragging: land it here. Released without moving: that was a CLICK, so the
        // line stays armed and waits for the second one.
        if (dragMoved)
        {
            if (linkTarget >= 0) linkStars(linkFrom, linkTarget);

            linkFrom = -1;
            linkTarget = -1;
        }
    }
    else if (grab == Grab::Node && !dragMoved && onNodeClicked != nullptr)
    {
        onNodeClicked(grabbedIndex);
    }

    grab = Grab::None;
    grabbedIndex = -1;
    grabbedObserver = -1;
    repaint();
}

void InvisConstellation::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (wheel.deltaY == 0.0f) return;

    const int star = hitTestNodeCore(e.position);
    if (star < 0) return;

    setNodeSensitivity(star, nodes[static_cast<size_t>(star)].sensitivity + wheel.deltaY * 0.06f);
}

void InvisConstellation::mouseExit(const juce::MouseEvent&)
{
    cursorInside = false;
    hoveredNode = -1;
    hoveredHub = -1;
    hoveredLink = -1;
    overBreakHandle = false;
    repaint();
}

void InvisConstellation::mouseMove(const juce::MouseEvent& e)
{
    cursorPos = e.position;
    cursorInside = true;

    // A GESTURE CANNOT OUTLIVE THE BUTTON THAT STARTED IT. mouseMove only arrives with nothing
    // held, so anything still grabbed here is stale - a press whose release went to a popup menu
    // instead of to us, say. Left alone it pins the readout on screen and steals the next drag.
    // Linking is the deliberate exception: it is armed precisely so it survives the button.
    if (grab != Grab::None && grab != Grab::Linking)
    {
        grab = Grab::None;
        grabbedIndex = -1;
        grabbedObserver = -1;
    }

    // An armed line follows the cursor with no button held - that is the whole point of arming it.
    if (linkFrom >= 0)
    {
        linkCursor = e.position;

        int over = hitTestNodeCore(e.position);
        if (over < 0) over = hitTestRim(e.position);

        linkTarget = (over >= 0 && canLink(linkFrom, over)) ? over : -1;
        repaint();
        return;
    }

    const int hub = hitTestHub(e.position);

    // The COLLAR counts as hovering the star. Requiring the cursor to be on the core before the
    // collar would even appear is why connecting was undiscoverable: the target only existed once
    // you were already somewhere else.
    int star = -1;
    bool onRim = false;
    if (hub < 0)
    {
        star = hitTestNodeCore(e.position);
        if (star < 0) { star = hitTestRim(e.position); onRim = (star >= 0); }
    }

    hoverOnRim = onRim;

    // Lines only offer themselves when nothing more specific is under the cursor
    juce::Point<float> onLink;
    const int link = (star >= 0 || hub >= 0 || hitTestObserver(e.position) >= 0)
                       ? -1 : hitTestLink(e.position, onLink);

    bool overHandle = false;
    if (link >= 0)
    {
        const auto& l = links[static_cast<size_t>(link)];
        const auto mid = (toPixels(nodes[static_cast<size_t>(l.a)].position)
                        + toPixels(nodes[static_cast<size_t>(l.b)].position)) * 0.5f;
        breakHandle = mid;
        overHandle = mid.getDistanceFrom(e.position) <= 8.0f;
    }

    // A hovered star redraws on every move: its socket silhouette sits at the angle you are
    // approaching from, so it has to follow the cursor rather than appear once.
    if (star < 0 && star == hoveredNode && link == hoveredLink && overHandle == overBreakHandle
        && hub == hoveredHub) return;

    hoveredHub = hub;
    hoveredNode = star;
    hoveredLink = link;
    overBreakHandle = overHandle;
    if (link >= 0) ghostPoint = onLink;

    repaint();
}

// --- Painting ---------------------------------------------------------------------------------

void InvisConstellation::paintGlassWell(juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto m = getMetrics(padSize);

    // The chart is a DISPLAY SET INTO THE CHASSIS, not a flat panel region. The recess, the sheen
    // and the vignette are what let the neon inside read as light behind glass rather than paint
    // on metal - which is what keeps it in the same world as the machined knobs around it.
    g.setColour(juce::Colours::black.withAlpha(0.75f));
    g.fillRoundedRectangle(area, m.corner);

    g.setGradientFill(juce::ColourGradient(
        juce::Colour::fromRGB(6, 9, 14), area.getCentreX(), area.getY(),
        juce::Colour::fromRGB(12, 16, 22), area.getCentreX(), area.getBottom(), false));
    g.fillRoundedRectangle(area.reduced(m.bezelWidth * 0.5f), m.corner);

    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.drawRoundedRectangle(area.reduced(0.5f), m.corner, m.bezelWidth * 0.8f);
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawRoundedRectangle(area.reduced(m.bezelWidth * 0.75f), m.corner * 0.8f, 1.0f);
}

void InvisConstellation::paintAura(juce::Graphics& g, const ConstellationNode& node, float weight)
{
    // A NEGATIVE STAR DOES NOT GLOW. It was previously given the same aura as a positive one, on
    // the reasoning that subtracting is still doing something - but light that means "removing
    // light" reads as nonsense. Inverted polarity is stated by the bar through the core instead,
    // and darkness is the honest picture of it.
    if (node.sensitivity < 0.0f) return;

    const auto centre = toPixels(node.position);
    const float r = radiusToPixels(node.radius);
    if (r <= 1.0f) return;

    // NO boundary ring. A drawn circle announces a hard edge the maths does not have - the falloff
    // is smooth all the way out - and it made the chart read as overlapping sets rather than as
    // light in a space.
    const float intensity = 0.09f + 0.50f * juce::jlimit(0.0f, 1.0f, weight);

    juce::ColourGradient field(node.colour.withAlpha(intensity), centre.x, centre.y,
                               juce::Colours::transparentBlack, centre.x + r, centre.y, true);

    // Stops shaped to match the smoothstep used for the weights, so what you see is the curve you
    // hear rather than a linear ramp standing in for it.
    field.addColour(0.30, node.colour.withAlpha(intensity * 0.72f));
    field.addColour(0.62, node.colour.withAlpha(intensity * 0.26f));
    field.addColour(0.85, node.colour.withAlpha(intensity * 0.06f));

    g.setGradientFill(field);
    g.fillEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);
}

void InvisConstellation::paintFigureHalo(juce::Graphics& g, const std::vector<int>& stars, float gate)
{
    if (stars.size() < 3) return;
    if (getFigureSensitivity(stars) < 0.0f) return;   // negative instrument: dark, see paintAura

    const auto area = getPadArea();
    const auto centre = toPixels(figureCentroid(nodes, stars));

    // Reach = the figure's own extent plus the shared halo, so the glow hugs the enclosure
    // instead of being a circle that happens to sit near it.
    float extent = 0.0f;
    std::vector<juce::Colour> colours;
    std::vector<float> weights;

    for (int idx : stars)
    {
        if (idx < 0 || idx >= static_cast<int>(nodes.size())) continue;

        const auto& node = nodes[static_cast<size_t>(idx)];
        extent = std::max(extent, toPixels(node.position).getDistanceFrom(centre));

        colours.push_back(node.colour);
        weights.push_back(1.0f);
    }

    if (colours.empty()) return;

    // ONE colour for one instrument - mixed on the hue wheel, so a red-green-blue trio glows
    // something you have to look at rather than the grey a channel average produced.
    const auto light = mixStarLight(colours, weights);
    const float reach = extent + figureRadius(nodes, stars) * area.getWidth();
    const float lit = juce::jlimit(0.0f, 1.0f, gate);

    // A CLOSED FIGURE SHOULD BE THE BRIGHTEST THING ON THE CHART. Suppressing the members' own
    // auras in favour of one wide field was correct in principle and far too dim in practice: the
    // same light spread over a much larger area reads as less light. So the field is layered -
    // a broad wash, an iridescent middle, and a lobe still sitting on every member.
    const float intensity = 0.10f + 0.60f * lit;

    juce::ColourGradient field(light.colour.withAlpha(intensity * 0.85f), centre.x, centre.y,
                               juce::Colours::transparentBlack, centre.x + reach, centre.y, true);

    // IRIDESCENCE. Members scattered around the hue wheel earn a field that travels between their
    // hues on its way out, instead of one flat tint - the nebula look the chart is asking for.
    // Members that agree on a hue get a plain gradient, because there is nothing to travel to.
    const float swing = light.spread * 0.5f;
    const float drift = std::sin(flowPhase * 0.9f) * 0.04f * light.spread;

    const auto shifted = [&](float turns, float alpha)
    {
        float h = light.colour.getHue() + turns + drift;
        h -= std::floor(h);
        return juce::Colour::fromHSV(h, light.colour.getSaturation(),
                                     light.colour.getBrightness(), alpha);
    };

    field.addColour(0.32, shifted(swing * 0.10f, intensity * 0.72f));
    field.addColour(0.58, shifted(swing * 0.06f, intensity * 0.40f));
    field.addColour(0.82, shifted(-swing * 0.08f, intensity * 0.14f));

    g.setGradientFill(field);
    g.fillEllipse(centre.x - reach, centre.y - reach, reach * 2.0f, reach * 2.0f);

    // Lobes on the members themselves. Each wears its OWN hue pulled most of the way toward the
    // mix: the instrument stays one colour, but you can still see what is in it, and the stars
    // light up as the observer arrives instead of the region brightening around them.
    for (int idx : stars)
    {
        if (idx < 0 || idx >= static_cast<int>(nodes.size())) continue;

        const auto& node = nodes[static_cast<size_t>(idx)];
        const auto at = toPixels(node.position);
        const float lobe = radiusToPixels(node.radius) * 0.72f;
        if (lobe <= 1.0f) continue;

        const auto tint = node.colour.interpolatedWith(light.colour, 0.62f);
        const float glow = intensity * 0.72f;

        juce::ColourGradient lamp(tint.withAlpha(glow), at.x, at.y,
                                  juce::Colours::transparentBlack, at.x + lobe, at.y, true);
        lamp.addColour(0.42, tint.withAlpha(glow * 0.44f));
        lamp.addColour(0.76, tint.withAlpha(glow * 0.12f));

        g.setGradientFill(lamp);
        g.fillEllipse(at.x - lobe, at.y - lobe, lobe * 2.0f, lobe * 2.0f);
    }

    // A bright core at the centroid: the instrument has a place, and saying so is what makes the
    // hub control look like it belongs to something.
    const float core = std::max(6.0f, extent * 0.28f);
    juce::ColourGradient heart(light.colour.brighter(0.4f).withAlpha(0.20f + 0.34f * lit),
                               centre.x, centre.y,
                               juce::Colours::transparentBlack, centre.x + core, centre.y, true);
    g.setGradientFill(heart);
    g.fillEllipse(centre.x - core, centre.y - core, core * 2.0f, core * 2.0f);
}

void InvisConstellation::paintFigureHub(juce::Graphics& g, const std::vector<int>& stars, float gate)
{
    if (stars.size() < 3) return;

    const auto m = getMetrics(padSize);
    const auto centre = toPixels(figureCentroid(nodes, stars));
    const float sens = getFigureSensitivity(stars);

    std::vector<juce::Colour> colours;
    for (int idx : stars)
        if (idx >= 0 && idx < static_cast<int>(nodes.size()))
            colours.push_back(nodes[static_cast<size_t>(idx)].colour);

    if (colours.empty()) return;

    const auto blend = mixStarLight(colours, {}).colour;
    const auto tint = sens < 0.0f ? juce::Colour::fromRGB(255, 23, 68) : blend;

    const bool active = (grab == Grab::Hub && grabbedIndex >= 0
                         && std::find(stars.begin(), stars.end(), grabbedIndex) != stars.end());
    const bool hovered = active
                       || (hoveredHub >= 0
                           && std::find(stars.begin(), stars.end(), hoveredHub) != stars.end());

    const float hub = m.nodeRadius * 0.9f;

    // ONE control for the whole instrument, sitting where the instrument is. The shared
    // sensitivity has no other honest home: the figure is derived from the lines, so a control
    // parked on any one member would look like it belonged to that star alone.
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillEllipse(centre.x - hub - 2.0f, centre.y - hub - 1.0f, (hub + 2.0f) * 2.0f, (hub + 2.0f) * 2.0f);

    const float ring = hub + 6.0f;
    const float start = juce::degreesToRadians(220.0f);
    const float end = juce::degreesToRadians(500.0f);
    const float mid = (start + end) * 0.5f;
    const float value = mid + sens * (end - mid);

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, ring, ring, 0.0f, start, end, true);
    g.setColour(juce::Colours::white.withAlpha(hovered ? 0.20f : 0.10f));
    g.strokePath(track, juce::PathStrokeType(m.arcStroke, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    juce::Path fill;
    fill.addCentredArc(centre.x, centre.y, ring, ring, 0.0f, std::min(mid, value), std::max(mid, value), true);
    g.setColour(tint.withAlpha(0.35f + 0.35f * std::abs(gate)));
    g.strokePath(fill, juce::PathStrokeType(m.arcStroke * 2.2f, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
    g.setColour(tint.brighter(0.3f));
    g.strokePath(fill, juce::PathStrokeType(m.arcStroke, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));

    InvisLED::drawLEDDot(g, centre, hub, tint, 0.30f + 0.85f * std::abs(gate) + (hovered ? 0.3f : 0.0f),
                         LEDMountType::RecessedSlot);

    if (sens < -0.01f)
    {
        const float bar = hub * 1.4f;
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.drawLine(centre.x - bar, centre.y, centre.x + bar, centre.y, 1.8f);
    }
}

void InvisConstellation::paintPolygon(juce::Graphics& g, const ChartFrame& frame)
{
    const auto m = getMetrics(padSize);
    const auto theme = getEffectiveTheme();
    const auto& contribs = frame.heard[0];
    const auto& figures = frame.figures;

    // A REFERENCE into the frame, deliberately. Taking `&cl` out of a range-for over the temporary
    // that getParallelClusters() returns left a pointer into a vector that died with the loop - a
    // use-after-free that crashed the moment a graph was closed.
    const auto& parallelClusters = frame.parallel;
    const auto area = getPadArea();

    // Closed clusters first: their wash is a backdrop the routing lines sit on top of. A cluster,
    // not a figure - a triangle with a star hung off it washes the TRIANGLE.
    for (const auto& cl : parallelClusters)
    {
        if (cl.stars.size() < 3) continue;

        juce::Path shape;
        juce::Point<float> centroid;
        bool started = false;
        int counted = 0;

        // Blend of the member stars, so the region wears the colour of what is actually in it
        std::vector<juce::Colour> colours;
        std::vector<float> shares;
        float activity = 0.0f;

        // The region the CYCLES enclose, not the hull of the members. Two loops sharing a side
        // have a hull that spans their outermost pair across sky nobody joined, and it was
        // flooding that wedge as though it were part of the instrument.
        shape = clusterRegion(nodes, links, cl.stars);
        shape.applyTransform(juce::AffineTransform::scale(area.getWidth(), area.getHeight())
                                 .translated(area.getX(), area.getY()));
        started = !shape.isEmpty();

        for (int idx : cl.stars)
        {
            if (idx < 0 || idx >= static_cast<int>(nodes.size())) continue;

            const auto& node = nodes[static_cast<size_t>(idx)];

            centroid += toPixels(node.position);
            ++counted;

            colours.push_back(node.colour);
            shares.push_back(0.25f + contribs[static_cast<size_t>(idx)].glow);
            activity = std::max(activity, contribs[static_cast<size_t>(idx)].glow);
        }

        if (!started || counted == 0) continue;

        centroid /= static_cast<float>(counted);

        // Mixed on the hue wheel and weighted by who is actually sounding, so the wash leans
        // toward whichever member the observer is favouring.
        const auto light = mixStarLight(colours, shares);
        const auto blend = light.colour;

        // CLOSED = parallel. It has to read as a REGION, not as a chain that happens to be idle -
        // that was the whole ambiguity: a dim loop looks like a serial figure with nothing
        // flowing, when in fact it has no direction to flow in at all.
        //
        // FILL ONLY. The hull used to be stroked as well, which drew every boundary edge TWICE -
        // once here and once as the member's own line - and the curved joins pulled this copy
        // inside the corners, so the pair separated into two visible rails. The region is the
        // wash; the lines are the lines you actually drew.
        //
        // And the wash is a FIELD, not a flat tint: five stars scattered around the hue wheel
        // average to one arbitrary middle hue, which at wash alpha over a dark chart is mud. A
        // gradient that drifts around that mean keeps the region reading as coloured light.
        float span = 1.0f;
        for (int idx : cl.stars)
            if (idx >= 0 && idx < static_cast<int>(nodes.size()))
                span = std::max(span, toPixels(nodes[static_cast<size_t>(idx)].position)
                                          .getDistanceFrom(centroid));

        const float wash = 0.075f + 0.11f * activity;
        const float turn = light.spread * 0.11f;

        const auto drifted = [&](float by, float alpha)
        {
            float h = blend.getHue() + by;
            h -= std::floor(h);
            return juce::Colour::fromHSV(h, blend.getSaturation(), blend.getBrightness(), alpha);
        };

        juce::ColourGradient field(drifted(turn, wash * 1.35f), centroid.x, centroid.y,
                                   drifted(-turn, wash * 0.55f), centroid.x + span, centroid.y, true);
        field.addColour(0.55, drifted(turn * 0.2f, wash));

        g.setGradientFill(field);
        g.fillPath(shape);
    }

    // Say the routing out loud. Everything above is a hint; a figure whose mode you have to infer
    // from a wash is a figure you will misread at least once.
    g.setFont(InvisFonts::getDisplayFont(m.labelFontSize * 0.85f));

    const auto sayRouting = [&](const juce::String& caption, juce::Point<float> at)
    {
        const auto box = juce::Rectangle<float>(at.x - 44.0f, at.y - 7.0f, 88.0f, 14.0f);

        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.drawText(caption, box.translated(0.0f, 0.7f), juce::Justification::centred, false);
        g.setColour(theme.textSecondary.withAlpha(0.55f));
        g.drawText(caption, box, juce::Justification::centred, false);
    };

    for (const auto& fig : figures)
    {
        // Number of STAGES, which is what "series" counts now - a triangle with a star hung off it
        // is two stages, not four.
        if (fig.clusters.size() > 1)
        {
            // Placed over the serial members rather than the figure centre, so the caption never
            // lands on top of a cluster's own wash and label.
            std::vector<int> loose;
            for (const auto& cl : fig.clusters)
                if (!cl.parallel) loose.insert(loose.end(), cl.stars.begin(), cl.stars.end());

            const auto& at = loose.empty() ? fig.stars : loose;
            sayRouting("SERIES " + juce::String(static_cast<int>(fig.clusters.size())),
                       toPixels(figureCentroid(nodes, at)));
        }

        // Pushed clear of the shared hub, which sits on the very same centroid - the caption was
        // being read straight through the control.
        for (const auto& cl : fig.clusters)
            if (cl.parallel)
                sayRouting("PARALLEL " + juce::String(static_cast<int>(cl.stars.size())),
                           toPixels(figureCentroid(nodes, cl.stars))
                               .translated(0.0f, m.nodeRadius * 0.9f + 20.0f));
    }

    for (size_t i = 0; i < links.size(); ++i)
    {
        const auto& l = links[i];
        if (l.a < 0 || l.b < 0) continue;
        if (l.a >= static_cast<int>(nodes.size()) || l.b >= static_cast<int>(nodes.size())) continue;

        const auto pa = toPixels(nodes[static_cast<size_t>(l.a)].position);
        const auto pb = toPixels(nodes[static_cast<size_t>(l.b)].position);

        const auto& ca = contribs[static_cast<size_t>(l.a)];
        const auto& cb = contribs[static_cast<size_t>(l.b)];

        // A line carries signal only when both ends are consecutive steps of the travelled path.
        const bool carrying = ca.chainOrder >= 0 && cb.chainOrder >= 0
                           && std::abs(ca.chainOrder - cb.chainOrder) == 1;

        const bool hovered = (static_cast<int>(i) == hoveredLink);

        // Lines INSIDE a closed cluster get the enclosure's own treatment rather than the chain
        // treatment - in parallel there is no hop from star to star to depict.
        //
        // They must still be DRAWN, though. Leaving them to the enclosure outline alone worked only
        // while every member happened to sit on the hull: drag one inside and it stops being a
        // vertex, so its lines vanished and the star looked cut loose from a figure it is still
        // part of. A line you drew never disappears because of where you moved a star.
        const ConstellationCluster* owner = nullptr;
        for (const auto& cl : parallelClusters)
        {
            const bool hasA = std::find(cl.stars.begin(), cl.stars.end(), l.a) != cl.stars.end();
            const bool hasB = std::find(cl.stars.begin(), cl.stars.end(), l.b) != cl.stars.end();
            if (hasA && hasB) { owner = &cl; break; }
        }

        if (owner != nullptr)
        {
            std::vector<juce::Colour> colours;
            std::vector<float> shares;
            float activity = 0.0f;

            for (int idx : owner->stars)
            {
                if (idx < 0 || idx >= static_cast<int>(nodes.size())) continue;
                colours.push_back(nodes[static_cast<size_t>(idx)].colour);
                shares.push_back(0.25f + contribs[static_cast<size_t>(idx)].glow);
                activity = std::max(activity, contribs[static_cast<size_t>(idx)].glow);
            }

            const auto blend = mixStarLight(colours, shares).colour;

            // EACH END KEEPS ITS OWN COLOUR. Painting every line in the mixed hue turned a figure
            // of five differently coloured stars into one olive cage - the mix is the identity of
            // the INSTRUMENT, and it has no business erasing what is joined into it. Pulled a
            // third of the way toward the blend, so the figure still reads as one thing.
            const auto ta = nodes[static_cast<size_t>(l.a)].colour.interpolatedWith(blend, 0.34f);
            const auto tb = nodes[static_cast<size_t>(l.b)].colour.interpolatedWith(blend, 0.34f);

            // A CLOSED CLUSTER IS ONE STAGE, so it does not pass a charge from star to star -
            // it all happens at once. The whole cage brightens on the beat instead, which is the
            // only honest picture of parallel: there is no order inside to depict.
            int stages = 0, order = -1;
            for (int idx : owner->stars)
                if (idx >= 0 && idx < static_cast<int>(nodes.size()))
                {
                    stages = frame.stages[static_cast<size_t>(idx)];
                    order = contribs[static_cast<size_t>(idx)].chainOrder;
                    break;
                }

            const float beat = stageFlash(stages, order);

            const float soft = (hovered ? 0.16f : 0.09f) + 0.10f * activity + 0.16f * beat;
            const float core = (hovered ? 0.62f : 0.34f) + 0.26f * activity + 0.40f * beat;

            juce::Path run;
            run.startNewSubPath(pa);
            run.lineTo(pb);

            g.setGradientFill({ ta.withAlpha(soft), pa, tb.withAlpha(soft), pb, false });
            g.strokePath(run, juce::PathStrokeType(m.polygonStroke * 5.0f));
            g.setGradientFill({ ta.withAlpha(core), pa, tb.withAlpha(core), pb, false });
            g.strokePath(run, juce::PathStrokeType(m.polygonStroke * 1.2f));
            continue;
        }

        if (!carrying)
        {
            g.setColour(theme.accentPrimary.withAlpha(hovered ? 0.42f : 0.18f));
            g.drawLine(pa.x, pa.y, pb.x, pb.y, m.polygonStroke);
        }
        else
        {
            const int downstream = (ca.chainOrder > cb.chainOrder) ? l.a : l.b;
            const auto colour = nodes[static_cast<size_t>(downstream)].colour;
            // The floor is what a chain looks like with nobody listening to it: dimmer, still
            // legible, still moving. Below about a third the spark stops reading as travel.
            const float energy = std::min(1.0f,
                std::abs(contribs[static_cast<size_t>(downstream)].amount) + 0.38f);

            g.setColour(colour.withAlpha(0.16f * energy));
            g.drawLine(pa.x, pa.y, pb.x, pb.y, m.polygonStroke * 6.0f);
            g.setColour(colour.withAlpha(0.55f * energy));
            g.drawLine(pa.x, pa.y, pb.x, pb.y, m.polygonStroke * 1.8f);

            // ONE CHARGE, CROSSING ONCE. A dashed pattern crawling along the line said "flow"
            // but never said WHERE the sound is, and its speed was set by the line's length -
            // drag a star further away and the same hop appeared to take longer.
            const auto from = (downstream == l.b) ? pa : pb;
            const auto to = (downstream == l.b) ? pb : pa;

            const int stages = frame.stages[static_cast<size_t>(downstream)];
            const int order = contribs[static_cast<size_t>(downstream)].chainOrder;

            // Scaled by how audible the hop is, but never off: a quiet chain still has to say
            // which way it runs. `energy` already carries a floor for exactly this reason.
            if (const float t = pulseOnSegment(stages, order); t >= 0.0f)
                drawSpark(g, from + (to - from) * t, colour, 2.6f, energy);
        }
    }

    // The break handle: hovering a line offers a cut without stealing the line's own click, which
    // inserts a star. Two different intents on the same object need two different targets.
    if (hoveredLink >= 0)
    {
        const float r = 7.0f;
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillEllipse(breakHandle.x - r, breakHandle.y - r, r * 2.0f, r * 2.0f);

        const auto tint = overBreakHandle ? juce::Colour::fromRGB(255, 23, 68)
                                          : theme.textSecondary.withAlpha(0.75f);
        g.setColour(tint);
        g.drawEllipse(breakHandle.x - r, breakHandle.y - r, r * 2.0f, r * 2.0f, 1.2f);

        const float c = r * 0.42f;
        g.drawLine(breakHandle.x - c, breakHandle.y - c, breakHandle.x + c, breakHandle.y + c, 1.6f);
        g.drawLine(breakHandle.x - c, breakHandle.y + c, breakHandle.x + c, breakHandle.y - c, 1.6f);
    }
}

void InvisConstellation::paintGhostNode(juce::Graphics& g)
{
    if (hoveredLink < 0 || overBreakHandle) return;
    if (static_cast<int>(nodes.size()) >= kMaxNodes) return;

    const auto m = getMetrics(padSize);
    const auto theme = getEffectiveTheme();

    // A silhouette, not a star: it must read as an OFFER. Drawing it solid would make the chart
    // look like it already has something you did not put there.
    g.setColour(theme.accentPrimary.withAlpha(0.14f));
    g.fillEllipse(ghostPoint.x - m.nodeRadius, ghostPoint.y - m.nodeRadius,
                  m.nodeRadius * 2.0f, m.nodeRadius * 2.0f);

    g.setColour(theme.accentPrimary.withAlpha(0.55f));
    g.drawEllipse(ghostPoint.x - m.nodeRadius, ghostPoint.y - m.nodeRadius,
                  m.nodeRadius * 2.0f, m.nodeRadius * 2.0f, 1.2f);

    const float cross = m.nodeRadius * 0.55f;
    g.drawLine(ghostPoint.x - cross, ghostPoint.y, ghostPoint.x + cross, ghostPoint.y, 1.2f);
    g.drawLine(ghostPoint.x, ghostPoint.y - cross, ghostPoint.x, ghostPoint.y + cross, 1.2f);
}

void InvisConstellation::paintSensitivityReadout(juce::Graphics& g, int index)
{
    const auto m = getMetrics(padSize);
    const auto& node = nodes[static_cast<size_t>(index)];
    const auto c = toPixels(node.position);

    // The value now lives IN the star as a fill, so this is only the exact number while you are
    // setting it. The ring that used to state it here belongs to the socket collar - two meanings
    // on the same circle is how connecting became invisible.
    const int pct = juce::roundToInt(node.sensitivity * 100.0f);
    const juce::String text = (pct > 0 ? "+" : "") + juce::String(pct) + "%";

    const auto box = juce::Rectangle<float>(c.x - 40.0f, c.y - m.nodeRadius - 24.0f, 80.0f, 14.0f);

    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.fillRoundedRectangle(box.reduced(22.0f, 0.0f).expanded(6.0f, 1.0f), 3.0f);

    g.setFont(InvisFonts::getDisplayFont(m.labelFontSize));
    g.setColour(node.sensitivity < 0.0f ? juce::Colour::fromRGB(255, 23, 68)
                                        : node.colour.brighter(0.4f));
    g.drawText(text, box, juce::Justification::centred, false);
}

void InvisConstellation::paintNode(juce::Graphics& g, int index, const ChartFrame& frame)
{
    const auto m = getMetrics(padSize);
    const auto& node = nodes[static_cast<size_t>(index)];
    const auto centre = toPixels(node.position);
    const bool hovered = (index == hoveredNode) || (grabbedIndex == index);

    // TETHER: deliberately overstated. This one line is the instrument saying "this is where your
    // sound is going, and this much of it".
    for (int p = 0; p < getNumObservers(); ++p)
    {
        const auto& contribs = frame.heard[static_cast<size_t>(p)];

        // The observer reaches only the FIRST stage: everything downstream is fed by the chain,
        // not by the listener, and a beam to it would misdescribe the routing.
        //
        // Reading that off the hop count says it exactly and for free. Re-deriving the whole
        // topology here to ask "is this star in a multi-stage figure" answered the same question
        // the snapshot had already answered - a single-stage figure leaves every member at hop
        // zero, so anything past zero is by definition downstream.
        if (contribs[static_cast<size_t>(index)].chainOrder > 0) continue;

        // Connection, not mix share: inside a closed figure the amounts are normalised across
        // the members, so a beam scaled by amount got THINNER the more stars joined the
        // instrument. The tether is about the link to the listener, so it follows glow.
        const float w = juce::jlimit(0.0f, 1.0f, contribs[static_cast<size_t>(index)].glow);
        if (w <= 0.004f) continue;

        const auto from = centre;
        const auto to = toPixels(observers[static_cast<size_t>(p)]);

        juce::Path beam;
        beam.startNewSubPath(from);
        beam.lineTo(to);

        g.setColour(node.colour.withAlpha(0.10f * w));
        g.strokePath(beam, juce::PathStrokeType(2.0f + 22.0f * w, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
        g.setColour(node.colour.withAlpha(0.22f * w));
        g.strokePath(beam, juce::PathStrokeType(1.5f + 10.0f * w, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
        g.setColour(node.colour.withAlpha(0.30f + 0.55f * w));
        g.strokePath(beam, juce::PathStrokeType(1.0f + 3.0f * w));

        // Segment 0 is the sound LEAVING the listener for the first stage, so the charge travels
        // from the observer toward the star - `from` is the star here, `to` the observer.
        const int stages = frame.stages[static_cast<size_t>(index)];

        if (const float t = pulseOnSegment(stages, 0); t >= 0.0f)
            drawSpark(g, to + (from - to) * t, node.colour, 2.4f + 1.6f * w, w);

        const float impact = 4.0f + 16.0f * w;
        g.setGradientFill(juce::ColourGradient(node.colour.withAlpha(0.42f * w), to.x, to.y,
                                                juce::Colours::transparentBlack, to.x + impact, to.y, true));
        g.fillEllipse(to.x - impact, to.y - impact, impact * 2.0f, impact * 2.0f);
    }

    // The core reflects the STRONGEST claim on it, so a star feeding one channel hard still reads
    // as active even when the other channel is nowhere near it.
    float peak = 0.0f;
    bool isEntry = false;
    for (int p = 0; p < getNumObservers(); ++p)
    {
        const auto& c = frame.heard[static_cast<size_t>(p)];
        peak = std::max(peak, juce::jlimit(0.0f, 1.0f, c[static_cast<size_t>(index)].glow));
        isEntry = isEntry || c[static_cast<size_t>(index)].isEntry;
    }

    // THE SOCKET COLLAR. It only appears on hover or while a line is in flight, because it is a
    // target, not decoration - a ring on every star permanently would bury the chart.
    const bool linking = (linkFrom >= 0);
    const bool candidate = linking && linkTarget == index;
    const float rim = m.nodeRadius + 7.0f;

    if (hovered || linking)
    {
        g.setColour(candidate ? juce::Colours::white.withAlpha(0.9f)
                              : node.colour.withAlpha(linking ? 0.30f : 0.42f));
        g.drawEllipse(centre.x - rim, centre.y - rim, rim * 2.0f, rim * 2.0f,
                      candidate ? 2.0f : 1.1f);
    }

    // A SOCKET, at the angle you are approaching from. The bare collar said "something happens
    // here" and left you to guess what; a point you can see yourself about to grab says it outright
    // - and it is the same point the line will leave from, so the gesture explains itself.
    if (hovered && hoverOnRim && !linking && cursorInside && index == hoveredNode)
    {
        const auto away = cursorPos - centre;
        const float len = away.getDistanceFrom({});

        if (len > 1.0f)
        {
            const auto at = centre + (away / len) * rim;
            const float dot = 4.0f;

            g.setColour(juce::Colours::black.withAlpha(0.55f));
            g.fillEllipse(at.x - dot - 1.0f, at.y - dot - 1.0f, (dot + 1.0f) * 2.0f, (dot + 1.0f) * 2.0f);
            g.setColour(node.colour.withAlpha(0.95f));
            g.fillEllipse(at.x - dot, at.y - dot, dot * 2.0f, dot * 2.0f);
            g.setColour(juce::Colours::white.withAlpha(0.8f));
            g.drawEllipse(at.x - dot, at.y - dot, dot * 2.0f, dot * 2.0f, 1.0f);
        }
    }

    // The entry star wears a gate ring: it is where the signal walks in, and that is the one piece
    // of routing the observer's position actually decides.
    if (isEntry)
    {
        const float gate = m.nodeRadius + 4.0f;
        g.setColour(node.colour.withAlpha(0.85f));
        g.drawEllipse(centre.x - gate, centre.y - gate, gate * 2.0f, gate * 2.0f, 1.6f);
    }

    // ARRIVAL. The star rings when the charge lands on it - sequentially down a chain, all at once
    // inside a closed cluster, because a cluster is one stage and its members share an order.
    const auto& own = frame.heard[0][static_cast<size_t>(index)];
    const float beat = stageFlash(frame.stages[static_cast<size_t>(index)], own.chainOrder);

    // Negative polarity stays unlit here too, so a star reads the same way as its aura does.
    const float lit = (node.sensitivity < 0.0f) ? 0.0f : peak;
    const float rNode = m.nodeRadius;

    // A HOLLOW STAR. The solid LED dome that used to fill the core is gone: it sat on top of the
    // one thing the core now has to say, and the fill was barely readable through it. What is left
    // is a piece of dark glass with a lit edge - the light lives in the aura, and the inside of the
    // star belongs to its value.
    g.setColour(juce::Colours::black.withAlpha(0.30f + 0.18f * (1.0f - lit)));
    g.fillEllipse(centre.x - rNode, centre.y - rNode, rNode * 2.0f, rNode * 2.0f);

    // SENSITIVITY IS HOW FULL THE STAR IS. An arc floating outside the core stated the value in
    // knob language, which cost the collar the one ring it needed for connecting AND made you hunt
    // for a thin line to read a number. Filling the star itself is legible at a glance from across
    // the chart, and it leaves the outside of the star for the socket.
    const float fill = juce::jlimit(0.0f, 1.0f, std::abs(node.sensitivity));

    if (fill > 0.004f)
    {
        const float pie = rNode * 0.88f;
        const float sweep = juce::MathConstants<float>::twoPi * fill;

        // Positive fills clockwise from twelve, negative anticlockwise: the direction alone tells
        // you the polarity before the colour or the bar does.
        const float from = (node.sensitivity < 0.0f) ? -sweep : 0.0f;

        juce::Path wedge;
        wedge.startNewSubPath(centre);
        wedge.addCentredArc(centre.x, centre.y, pie, pie, 0.0f, from, from + sweep, false);
        wedge.closeSubPath();

        const auto tint = node.sensitivity < 0.0f ? juce::Colour::fromRGB(255, 23, 68)
                                                  : node.colour;

        // Solid enough to read against the dark glass at a glance, brighter still when the star is
        // actually being fed.
        g.setColour(tint.withAlpha(0.62f + 0.38f * lit));
        g.fillPath(wedge);

        // The cut edge, so a nearly-full star is not mistaken for a full one
        if (fill < 0.998f)
        {
            const float edge = from + sweep;
            g.setColour(juce::Colours::black.withAlpha(0.55f));
            g.drawLine(centre.x, centre.y,
                       centre.x + std::sin(edge) * pie, centre.y - std::cos(edge) * pie, 1.6f);
        }
    }

    // The lit edge of the glass. This is the whole star when sensitivity is zero, so it can never
    // fade out entirely - an invisible star is one you cannot pick up again.
    g.setColour(node.colour.withAlpha(std::min(1.0f, 0.45f + 0.45f * lit + 0.45f * beat
                                                     + (hovered ? 0.15f : 0.0f))));
    g.drawEllipse(centre.x - rNode, centre.y - rNode, rNode * 2.0f, rNode * 2.0f,
                  hovered ? 1.8f : 1.3f + 1.4f * beat);

    // The ring the strike throws off, so the beat carries across the chart at a glance.
    if (beat > 0.01f)
    {
        const float ring = rNode * (1.0f + 0.85f * (1.0f - beat));
        g.setColour(node.colour.withAlpha(0.55f * beat));
        g.drawEllipse(centre.x - ring, centre.y - ring, ring * 2.0f, ring * 2.0f, 1.6f * beat);

        const float halo = rNode * 2.6f;
        g.setGradientFill(juce::ColourGradient(node.colour.withAlpha(0.30f * beat), centre.x, centre.y,
                                               juce::Colours::transparentBlack,
                                               centre.x + halo, centre.y, true));
        g.fillEllipse(centre.x - halo, centre.y - halo, halo * 2.0f, halo * 2.0f);
    }

    // A single specular arc across the top keeps it reading as glass rather than as a flat hole.
    juce::Path sheen;
    sheen.addCentredArc(centre.x, centre.y, rNode * 0.78f, rNode * 0.78f, 0.0f,
                        juce::degreesToRadians(-58.0f), juce::degreesToRadians(24.0f), true);
    g.setColour(juce::Colours::white.withAlpha(0.22f));
    g.strokePath(sheen, juce::PathStrokeType(1.2f));

    // A negative star subtracts rather than adds, so it gets a bar through it - polarity is the
    // one thing you must never have to guess about.
    if (node.sensitivity < -0.01f)
    {
        const float bar = m.nodeRadius * 1.5f;
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.drawLine(centre.x - bar, centre.y, centre.x + bar, centre.y, 1.8f);
    }

    if (node.label.isNotEmpty())
    {
        g.setFont(InvisFonts::getDisplayFont(m.labelFontSize));
        const auto textArea = juce::Rectangle<float>(centre.x - 60.0f,
                                                     centre.y + m.nodeRadius + 5.0f, 120.0f, 14.0f);

        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawText(node.label.toUpperCase(), textArea.translated(0.0f, 0.7f),
                   juce::Justification::centred, false);
        g.setColour(node.colour.brighter(0.3f).withAlpha(0.65f + 0.35f * peak));
        g.drawText(node.label.toUpperCase(), textArea, juce::Justification::centred, false);
    }
}

void InvisConstellation::paintObserver(juce::Graphics& g, int observerIndex)
{
    const auto m = getMetrics(padSize);
    const auto c = toPixels(observers[static_cast<size_t>(observerIndex)]);
    const float r = m.observerRadius;
    const auto colour = getObserverColour(observerIndex);
    const bool held = (grab == Grab::Observer && grabbedObserver == observerIndex);

    // A RETICLE, not a dot. The observer is the single most important thing on the chart - it is
    // where you are - and it has to stay findable across a field of glowing stars and beams.
    const float reach = r * 4.2f;
    const float gap = r * 1.5f;
    const float alpha = held ? 0.95f : 0.62f;

    for (int pass = 0; pass < 2; ++pass)
    {
        const float w = (pass == 0) ? 3.2f : 1.4f;
        g.setColour(pass == 0 ? juce::Colours::black.withAlpha(0.5f) : colour.withAlpha(alpha));

        g.drawLine(c.x - reach, c.y, c.x - gap, c.y, w);
        g.drawLine(c.x + gap, c.y, c.x + reach, c.y, w);
        g.drawLine(c.x, c.y - reach, c.x, c.y - gap, w);
        g.drawLine(c.x, c.y + gap, c.x, c.y + reach, w);
    }

    const float ring = r * 2.4f;
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.drawEllipse(c.x - ring, c.y - ring, ring * 2.0f, ring * 2.0f, 2.6f);
    g.setColour(colour.withAlpha(alpha * 0.85f));
    g.drawEllipse(c.x - ring, c.y - ring, ring * 2.0f, ring * 2.0f, 1.2f);

    // Ticks on the diagonals so they never sit under the cross-hairs
    for (int i = 0; i < 4; ++i)
    {
        const float a = juce::MathConstants<float>::pi * (0.25f + 0.5f * static_cast<float>(i));
        const float ca = std::cos(a), sa = std::sin(a);
        g.drawLine(c.x + ca * ring * 0.82f, c.y + sa * ring * 0.82f,
                   c.x + ca * ring * 1.18f, c.y + sa * ring * 1.18f, 1.4f);
    }

    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillEllipse(c.x - r - 1.5f, c.y - r - 1.0f, (r + 1.5f) * 2.0f, (r + 1.5f) * 2.0f);

    InvisLED::drawLEDDot(g, c, r, colour, held ? 1.6f : 1.15f, LEDMountType::ProtrudingDome);

    juce::String a, b;
    getConstellationObserverLabels(channelMode, a, b);
    const juce::String label = (observerIndex == 0) ? a : b;

    if (label.isNotEmpty())
    {
        g.setFont(InvisFonts::getDisplayFont(m.labelFontSize));
        const auto box = juce::Rectangle<float>(c.x - 30.0f, c.y - ring - 15.0f, 60.0f, 12.0f);

        g.setColour(juce::Colours::black.withAlpha(0.65f));
        g.drawText(label, box.translated(0.0f, 0.7f), juce::Justification::centred, false);
        g.setColour(colour.brighter(0.35f));
        g.drawText(label, box, juce::Justification::centred, false);
    }
}

void InvisConstellation::paint(juce::Graphics& g)
{
    const auto content = centreIntrinsic(getLocalBounds().toFloat(), getIntrinsicSize());
    if (content.getWidth() <= 0.0f) return;

    paintGlassWell(g, content);

    // ONE derivation for the whole frame - see ChartFrame.
    const auto frame = takeSnapshot();
    const auto& contribs = frame.heard[0];

    // Halos first: they are the field everything else sits in, and drawing them under the lines
    // keeps the figure legible where several fields overlap.
    //
    // A CLOSED figure gets ONE shared halo instead of its members' individual ones. Drawing both
    // would contradict the routing: the figure is a single instrument with a single gate, and
    // showing each member with its own field would say the opposite.
    const auto& parallelClusters = frame.parallel;
    std::vector<bool> inClosed(nodes.size(), false);

    for (const auto& cl : parallelClusters)
    {
        if (cl.stars.size() < 3) continue;

        // The figure's own gate - how close the observer is to the ENCLOSURE. Summing the members'
        // amounts gave the same answer only by accident of the shares totalling one, and it
        // collapsed the moment polarity or an equal-share fallback entered the picture.
        float gate = 0.0f;
        for (int idx : cl.stars)
            if (idx >= 0 && idx < static_cast<int>(nodes.size()))
            {
                inClosed[static_cast<size_t>(idx)] = true;
                gate = std::max(gate, contribs[static_cast<size_t>(idx)].glow);
            }

        paintFigureHalo(g, cl.stars, std::min(1.0f, gate));
    }

    for (size_t i = 0; i < nodes.size(); ++i)
        if (!inClosed[i])
            paintAura(g, nodes[i], contribs[i].glow);

    paintPolygon(g, frame);
    paintGhostNode(g);

    // The line in flight. Keyed on linkFrom, not on a held button: once armed it follows the
    // cursor until you land it or click it away.
    if (linkFrom >= 0 && linkFrom < static_cast<int>(nodes.size()))
    {
        const auto from = toPixels(nodes[static_cast<size_t>(linkFrom)].position);
        const auto tint = nodes[static_cast<size_t>(linkFrom)].colour;

        g.setColour(tint.withAlpha(linkTarget >= 0 ? 0.85f : 0.40f));

        const float dash[] = { 5.0f, 4.0f };
        juce::Path line, dashed;
        line.startNewSubPath(from);
        line.lineTo(linkCursor);
        juce::PathStrokeType(1.6f).createDashedStroke(dashed, line, dash, 2);
        g.strokePath(dashed, juce::PathStrokeType(linkTarget >= 0 ? 2.2f : 1.6f));

        // Say what the next click does. A line hanging off the cursor with no explanation is the
        // same discoverability problem one step further along.
        const auto m = getMetrics(padSize);
        g.setFont(InvisFonts::getDisplayFont(m.labelFontSize * 0.85f));
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.drawText(linkTarget >= 0 ? "CLICK TO JOIN" : "CLICK A STAR - OR ANYWHERE TO CANCEL",
                   juce::Rectangle<float>(linkCursor.x - 90.0f, linkCursor.y + 12.0f, 180.0f, 13.0f),
                   juce::Justification::centred, false);
    }

    for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
        paintNode(g, i, frame);

    // Only while you are setting it: the star's own fill states the value the rest of the time.
    if (grab == Grab::Sensitivity && grabbedIndex >= 0)
        paintSensitivityReadout(g, grabbedIndex);

    for (const auto& cl : parallelClusters)
    {
        if (cl.stars.size() < 3) continue;

        float gate = 0.0f;
        for (int idx : cl.stars)
            if (idx >= 0 && idx < static_cast<int>(nodes.size()))
                gate = std::max(gate, contribs[static_cast<size_t>(idx)].glow);

        paintFigureHub(g, cl.stars, std::min(1.0f, gate));
    }

    for (int p = 0; p < getNumObservers(); ++p)
        paintObserver(g, p);

    // Glass sheen last, over everything: a highlight things can be drawn on top of is paint, not
    // glass.
    const auto m = getMetrics(padSize);
    const auto glass = content.reduced(m.bezelWidth);
    g.setGradientFill(juce::ColourGradient(
        juce::Colours::white.withAlpha(0.055f), glass.getX(), glass.getY(),
        juce::Colours::transparentBlack, glass.getX() + glass.getWidth() * 0.55f,
        glass.getY() + glass.getHeight() * 0.45f, false));
    g.fillRoundedRectangle(glass, m.corner * 0.8f);
}

} // namespace invis::ui
