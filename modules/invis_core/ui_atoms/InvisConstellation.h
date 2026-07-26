#pragma once

#include "InvisLED.h"
#include "../design_system/InvisThemeSupplier.h"
#include "../design_system/InvisLayout.h"
#include "../design_system/InvisFonts.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <array>
#include <vector>

namespace invis::ui {

enum class InvisConstellationSize {
    S,
    M, // Standard workspace pad (default)
    L
};

/**
 * How the pad's position(s) map onto the stereo signal.
 *
 * `MidSideLinked` deserves a warning. For a LINEAR, time-invariant node it is mathematically
 * identical to `Linked`: with M=(L+R)/2 and S=(L-R)/2, applying the same linear P to both gives
 * L'=P(M)+P(S)=P(L) and R'=P(M)-P(S)=P(R) - the encode and decode cancel out exactly.
 *
 * It only does something when the node is NON-LINEAR (saturation, compression - non-linearity
 * does not commute with the M/S matrix) or STATEFUL (reverbs, delays, modulated effects, where
 * separate instances on M and S decorrelate). Since that describes almost every effect planned
 * here, the mode earns its place - but a purely linear node in this mode is a no-op, and the UI
 * should eventually say so rather than implying otherwise.
 */
enum class ConstellationChannelMode {
    Linked,         // L+R, one position
    LeftRight,      // L and R positioned independently
    MidSideLinked,  // M+S, one position - see the warning above
    MidSide         // M and S positioned independently
};

inline const char* getConstellationChannelModeName(ConstellationChannelMode m)
{
    switch (m)
    {
        case ConstellationChannelMode::LeftRight:     return "L R";
        case ConstellationChannelMode::MidSideLinked: return "M+S";
        case ConstellationChannelMode::MidSide:       return "M S";
        case ConstellationChannelMode::Linked:
        default:                              return "L+R";
    }
}

/** Two positions, or one? */
inline bool constellationModeHasTwoObservers(ConstellationChannelMode m)
{
    return m == ConstellationChannelMode::LeftRight || m == ConstellationChannelMode::MidSide;
}

/** Short captions for the observers in the current mode. */
inline void getConstellationObserverLabels(ConstellationChannelMode m, juce::String& a, juce::String& b)
{
    switch (m)
    {
        case ConstellationChannelMode::LeftRight:     a = "L"; b = "R"; break;
        case ConstellationChannelMode::MidSide:       a = "M"; b = "S"; break;
        case ConstellationChannelMode::MidSideLinked: a = "M+S"; b = {}; break;
        case ConstellationChannelMode::Linked:
        default:                              a = {};    b = {}; break;
    }
}

struct ConstellationMetrics {
    float bezelWidth;
    float corner;
    float nodeRadius;      // the solid core you grab to move a node
    float observerRadius;
    float arcRadius;       // sensitivity arc, drawn around the node core
    float arcStroke;
    float polygonStroke;
    float labelFontSize;
};

/**
 * One star of the constellation. Positions and radii are NORMALIZED (0..1 across the pad), so
 * the geometry survives a resize, a zoom change and a preset recall without rescaling.
 */
struct ConstellationNode {
    juce::Point<float> position { 0.5f, 0.5f };
    float radius { 0.26f };       // spatial reach of the halo, normalized to chart width

    /**
     * BIPOLAR contribution, -1..+1.
     *
     * Positive blends the wet signal in; NEGATIVE blends it in inverted, so the stage subtracts
     * rather than adds - comb filtering, cancellation, "negative" ambience. That is a real
     * production technique, not a novelty, which is why the control is signed rather than 0..1.
     *
     * In a SERIAL chain this is the stage's dry/wet against whatever already reached it.
     * In a PARALLEL figure it scales the distance-derived weight.
     */
    float sensitivity { 1.0f };

    juce::Colour colour { juce::Colour::fromRGB(0, 229, 255) };
    juce::String label;
};

/** One drawn line between two stars. */
struct StarLink {
    int a { -1 };
    int b { -1 };

    bool touches(int i) const { return a == i || b == i; }
    int other(int i) const { return a == i ? b : a; }
    bool same(int x, int y) const { return (a == x && b == y) || (a == y && b == x); }
};

/**
 * A figure, DERIVED from the links rather than stored.
 *
 * This is the whole fix for the old editor. Storing branch membership and deriving the lines made
 * topology a consequence of the order you happened to click in: you could never join two stars
 * that already existed, never separate them, and "closed" had to be a toggle because there was no
 * figure to select. Storing the LINKS and deriving the figures inverts that - the drawing is the
 * data, and every figure recomputes itself the moment a line is drawn or cut.
 *
 * THE LAW, and it holds for any shape you can draw:
 *   ANYTHING SERIAL IS SERIAL. Signal enters at the star nearest the observer and walks onward.
 *   ANYTHING CLOSED BEHAVES AS ONE NODE, with everything inside it in parallel.
 *
 * The two are not alternatives at the level of a whole component - they compose. Hang a star off a
 * triangle and you have a parallel trio feeding a serial stage, because that is what you drew. See
 * ConstellationFigure for how a component is cut into those stages.
 *
 * So closing a figure is not a button: you draw the line back to the first star, which is exactly
 * what drawing a constellation is.
 */
/**
 * A CLUSTER is what the routing treats as ONE node.
 *
 * A lone star is a cluster of one. A CYCLE collapses into a single cluster whose members all sound
 * together - that is the whole meaning of closing a figure.
 */
struct ConstellationCluster {
    std::vector<int> stars;
    bool parallel { false };      // true when this cluster is a closed group, not a lone star
};

/**
 * One connected component of the chart, decomposed into clusters.
 *
 * The decomposition is what makes the law work on real drawings. Judging a whole COMPONENT as
 * closed-or-not was wrong: hang one star off a triangle and the component contains a cycle, so the
 * entire thing - tail included - became a four-star parallel blob. What you drew was a triangle
 * with a branch, and that is what it must be.
 *
 * So the component is cut at its BRIDGES - the lines that carry the whole signal because removing
 * them would sever the figure. What survives the cuts are the cycles, each collapsing to one
 * parallel cluster; the bridges become the serial hops between clusters. Contracting cycles this
 * way always leaves a TREE, so a component is always a serial arrangement of clusters, whatever
 * shape you drew.
 */
struct ConstellationFigure {
    std::vector<int> stars;                          // every member, discovery order
    std::vector<ConstellationCluster> clusters;      // the routing stages
    std::vector<std::pair<int, int>> clusterLinks;   // bridge tree, indices into `clusters`
};

/** What one star does for the current observer position. */
struct StarContribution {
    float amount { 0.0f };    // signed: magnitude is dry/wet, sign is polarity

    /**
     * How brightly this star should read, 0..1 - deliberately SEPARATE from `amount`.
     *
     * Inside a closed figure the audio amounts are normalised so the members share one gate, which
     * means a four-star instrument gives each member a quarter. That is correct for sound and a
     * lie on screen: standing deep inside an instrument, it looked like everything had faded out.
     * Glow therefore follows how strongly the OBSERVER is connected, not the member's share of
     * the mix.
     */
    float glow { 0.0f };

    int chainOrder { -1 };    // 0 = entry point, -1 = parallel or silent
    bool isEntry { false };
};

/**
 * Everything one frame needs to know about the chart, worked out once.
 *
 * Routing is DERIVED, not stored, so asking any question about it re-derives the whole thing:
 * bridges, clusters, entry, the lot. Painting used to ask that question per star AND per observer,
 * which on a chart of eight stars meant around forty full decompositions per frame for an answer
 * that cannot change within the frame. Deriving it once and passing it down is also what keeps the
 * painters honest - a painter that cannot re-derive cannot quietly disagree with the routing.
 */
struct ChartFrame {
    std::array<std::vector<StarContribution>, 2> heard;   // one row per observer
    std::vector<ConstellationFigure> figures;
    std::vector<ConstellationCluster> parallel;           // the closed clusters, flattened
};

/**
 * CONSTELLATION - the star-chart effect editor.
 *
 * Each node is a STAR, and a star is an effect. The polygon joining them is the asterism: its
 * ORDER is meaningful, which is why a new star is inserted on an EDGE rather than dropped
 * anywhere. The observer is where you are standing, and a star's weight is how brightly it
 * shines from there. A star's contribution is HOW DEEP
 * THE OBSERVER SITS INSIDE ITS HALO - full at the node itself, zero at the aura boundary, zero
 * outside it.
 *
 * WEIGHTS ARE NOT NORMALIZED. A vector mixer normalises so the weights always sum to 1, which
 * means you can never reach dry signal - you only ever redistribute between effects. Here the
 * aura radius is the sensitivity control, so it has to actually decide something: park the observer
 * outside every aura and you hear the source untouched. That dry zone is the whole point of
 * having a radius at all.
 *
 * Interaction:
 *   - drag a node CORE            move the effect
 *   - ALT / right-drag on a node  adjust its sensitivity, knob-style (vertical)
 *   - wheel over a node           same, in steps
 *   - drag the PUCK               move the balance. It CANNOT be teleported by clicking: the
 *                                 listener position is where you are standing, and standing
 *                                 somewhere is a deliberate act, not a side effect of a stray click
 *   - click a polygon EDGE        insert a node there; hovering the edge previews it as a ghost
 *
 * A plain drag cannot both move a node and turn its sensitivity, so the second gesture takes a
 * modifier. That is the convention wherever a control has a position AND a size.
 *
 * Presentational and stateless: it holds geometry and reports changes. It knows nothing about
 * what an "effect" is, nothing about APVTS, and performs no audio.
 */
class InvisConstellation : public juce::Component, public InvisThemeSupplier {
public:
    InvisConstellation();
    ~InvisConstellation() override = default;

    static constexpr int kMaxNodes = 8;
    static constexpr float kMinRadius = 0.06f;
    static constexpr float kMaxRadius = 0.72f;

    /** New node created by clicking an edge. Reports where it landed in the node order. */
    std::function<void(int index)> onNodeInserted;

    static ConstellationMetrics getMetrics(InvisConstellationSize size);
    static juce::Point<int> getIntrinsicSize(InvisConstellationSize size);
    juce::Point<int> getIntrinsicSize() const { return getIntrinsicSize(padSize); }

    void setBoundsCentredIn(juce::Rectangle<int> area)
    {
        setBounds(centreIntrinsic(area, getIntrinsicSize()));
    }

    void setPadSize(InvisConstellationSize size) { padSize = size; repaint(); }

    // --- Nodes ---
    int addNode(const juce::String& label, juce::Colour colour);
    void removeNode(int index);
    void clearNodes();
    int getNumNodes() const { return static_cast<int>(nodes.size()); }
    const ConstellationNode& getNode(int index) const { return nodes[static_cast<size_t>(index)]; }

    /**
     * Scatters the stars. Positions are spread around the chart with a jittered angular sweep
     * rather than uniformly at random: pure uniform noise clusters and leaves bald patches, so
     * half the throws would produce a figure with stars piled on top of each other.
     */
    void randomise();

    void setNodePosition(int index, juce::Point<float> normalized);
    void setNodeRadius(int index, float normalizedRadius);

    /** Identity of a star. The atom stores these but assigns no meaning to them - what a star IS
        belongs to the product, not to the chart. */
    void setNodeLabel(int index, const juce::String& label);
    void setNodeColour(int index, juce::Colour colour);

    // --- Channel mode ---
    void setChannelMode(ConstellationChannelMode mode);
    ConstellationChannelMode getChannelMode() const { return channelMode; }
    int getNumObservers() const { return constellationModeHasTwoObservers(channelMode) ? 2 : 1; }

    // --- Observers ---
    void setObserverPosition(int observerIndex, juce::Point<float> normalized);
    juce::Point<float> getObserverPosition(int observerIndex = 0) const
    {
        return observers[static_cast<size_t>(juce::jlimit(0, 1, observerIndex))];
    }

    /**
     * Contribution of every node at the current observer position, in node order.
     *
     * Pure and static so the DSP side can compute the SAME numbers from the same geometry without
     * a UI object in the signal path - the pad must never become the source of truth for audio.
     */
    static std::vector<float> computeWeights(const std::vector<ConstellationNode>& nodes,
                                             juce::Point<float> observerPosition);

    /**
     * Full routing evaluation: who is in a chain, where the signal enters, and what each star
     * contributes. Pure and static so the DSP computes the same answer from the same geometry.
     *
     * @param previousEntry  the entry star chosen last time, or -1. Passing it back in engages
     *                       HYSTERESIS: the entry only changes once a rival is clearly nearer.
     *                       Without it, an observer sitting on the midline between two stars
     *                       flips the whole processing order back and forth - an audible click,
     *                       and the reason this parameter exists at all.
     */
    /** Connected components of the link graph, ordered. Pure, so the DSP derives the same shape. */
    static std::vector<ConstellationFigure> buildFigures(size_t numNodes,
                                                         const std::vector<StarLink>& links);

    static std::vector<StarContribution> evaluate(const std::vector<ConstellationNode>& nodes,
                                                  const std::vector<StarLink>& links,
                                                  juce::Point<float> observerPosition,
                                                  int previousEntry = -1);

    std::vector<float> getWeights(int observerIndex = 0) const
    {
        return computeWeights(nodes, getObserverPosition(observerIndex));
    }

    std::vector<StarContribution> getContributions(int observerIndex = 0) const;

    // --- Links ---
    //
    // A star may take ANY number of lines. The earlier two-socket cap existed to guarantee every
    // component came out as a path or a cycle; with branching allowed the rule is simply stated
    // instead: a component containing a CYCLE behaves as one parallel instrument, and a component
    // without one is traversed in series, branches included.

    const std::vector<StarLink>& getLinks() const { return links; }
    std::vector<ConstellationFigure> getFigures() const { return buildFigures(nodes.size(), links); }

    /** Geometry of a figure, used for the shared halo and the hub control. */
    static juce::Point<float> figureCentroid(const std::vector<ConstellationNode>& nodes,
                                             const std::vector<int>& stars);

    /**
     * The region a closed cluster ENCLOSES, in normalized space.
     *
     * Not the convex hull. The hull invents edges: join five stars into two loops that share a
     * side and the hull spans the outermost pair even when you never drew a line between them,
     * flooding a wedge of empty sky that nothing encloses. What a figure actually encloses is the
     * union of the areas bounded by its CYCLES, so that is what this returns - one counter-
     * clockwise subpath per cycle, unioned by non-zero winding.
     */
    static juce::Path clusterRegion(const std::vector<ConstellationNode>& nodes,
                                    const std::vector<StarLink>& links,
                                    const std::vector<int>& stars);

    /** 0 inside the region the cluster encloses, otherwise the distance out to its nearest line. */
    static float distanceToFigure(const std::vector<ConstellationNode>& nodes,
                                  const std::vector<StarLink>& links,
                                  const std::vector<int>& stars,
                                  juce::Point<float> p);

    /** Mean halo radius of a figure's members - its shared reach. */
    static float figureRadius(const std::vector<ConstellationNode>& nodes,
                              const std::vector<int>& stars);

    /** Every parallel cluster on the chart, flattened - what the halos and hubs are drawn for. */
    std::vector<ConstellationCluster> getParallelClusters() const;

    /**
     * Shared sensitivity of a closed figure. Stored on the member STARS and kept in sync rather
     * than on the figure: figures are derived from the links and have no stable identity to hang
     * state on, so anything kept per-figure would be lost the moment a line moved. Breaking the
     * ring then simply leaves each star holding the value it had.
     */
    void setFigureSensitivity(int anyMemberIndex, float bipolarAmount);
    float getFigureSensitivity(const std::vector<int>& stars) const;

    bool canLink(int a, int b) const;
    bool linkStars(int a, int b);
    void unlinkStars(int a, int b);
    int countLinks(int starIndex) const;

    void setNodeSensitivity(int index, float bipolarAmount);

    /** Geometry changed (node moved / resized / added / removed). */
    std::function<void()> onGeometryChanged;

    /** An observer moved, or geometry changed under it. Carries which observer and its fresh weights. */
    std::function<void(int observerIndex, const std::vector<float>&)> onWeightsChanged;

    /** A node's core was clicked without dragging - the host may open its effect editor. */
    std::function<void(int index)> onNodeClicked;

    InvisTheme getEffectiveTheme() const override
    {
        return customTheme.value_or(InvisThemeSupplier::getParentTheme(this));
    }

    void setThemeOverride(const InvisTheme& theme) { customTheme = theme; repaint(); }

    /** Advances the beam flow animation. Drive from the editor's timer. */
    void tickAnimation(float deltaTimeSeconds = 0.016f);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

private:
    enum class Grab { None, Node, Sensitivity, Observer, Linking, Hub };

    juce::Rectangle<float> getPadArea() const;
    juce::Point<float> toPixels(juce::Point<float> normalized) const;
    juce::Point<float> toNormalized(juce::Point<float> pixels) const;
    float radiusToPixels(float normalizedRadius) const;

    int hitTestNodeCore(juce::Point<float> pixels) const;
    int hitTestObserver(juce::Point<float> pixels) const;   // -1, 0 or 1
    juce::Colour getObserverColour(int observerIndex) const;

    /** Nearest polygon edge under the cursor, and where on it. -1 if none is close enough. */
    int hitTestLink(juce::Point<float> pixels, juce::Point<float>& pointOnLink) const;

    /** The socket collar: grabbing here draws a line instead of moving the star. */
    int hitTestRim(juce::Point<float> pixels) const;

    /** The hub at a closed figure's centre: one control for the whole instrument. Returns the
        index of any member star, which is how the figure is addressed. */
    int hitTestHub(juce::Point<float> pixels) const;

    /** Inserts a node BETWEEN vertex `edgeIndex` and the next one, keeping the polygon's shape. */
    void insertNodeOnLink(int linkIndex, juce::Point<float> normalized);

    void notifyWeights();

    void paintGlassWell(juce::Graphics& g, juce::Rectangle<float> area);
    void paintAura(juce::Graphics& g, const ConstellationNode& node, float weight);
    void paintPolygon(juce::Graphics& g, const ChartFrame& frame);
    void paintNode(juce::Graphics& g, int index, const ChartFrame& frame);
    void paintSensitivityReadout(juce::Graphics& g, int index);
    void paintFigureHalo(juce::Graphics& g, const std::vector<int>& stars, float gate);
    void paintFigureHub(juce::Graphics& g, const std::vector<int>& stars, float gate);

    /** The whole frame's view of the chart, so nothing below re-derives the routing. */
    ChartFrame takeSnapshot() const;
    void paintGhostNode(juce::Graphics& g);
    void paintObserver(juce::Graphics& g, int observerIndex);

    InvisConstellationSize padSize { InvisConstellationSize::M };

    std::vector<ConstellationNode> nodes;
    std::vector<StarLink> links;
    mutable int lastEntry[2] { -1, -1 };   // per observer, for entry hysteresis

    ConstellationChannelMode channelMode { ConstellationChannelMode::Linked };
    juce::Point<float> observers[2] { { 0.5f, 0.5f }, { 0.5f, 0.5f } };
    int grabbedObserver { -1 };

    Grab grab { Grab::None };
    int grabbedIndex { -1 };
    int hoveredNode { -1 };
    int hoveredHub { -1 };
    bool dragMoved { false };
    float grabStartRadius { 0.0f };
    juce::Point<float> grabStartPos;

    float flowPhase { 0.0f };
    juce::Random rng;

    // Ghost insertion preview, and the break handle on a hovered link
    int hoveredLink { -1 };
    juce::Point<float> ghostPoint;
    juce::Point<float> breakHandle;
    bool overBreakHandle { false };

    // In-progress link.
    //
    // ARMED, not dragged. Press-and-drag was the whole reason nobody could find the gesture: you
    // had to guess that the thin collar around a star meant anything, and then hold the button the
    // entire way across the chart. Now a click on the collar arms the line, it follows the cursor
    // with the button up, and a second click lands it - or a click on nothing throws it away.
    int linkFrom { -1 };
    juce::Point<float> linkCursor;
    int linkTarget { -1 };

    // Where the cursor is, so a hovered star can show its socket at the angle you approached from.
    // `hoverOnRim` separates the two halves of a star: the COLLAR offers a connection, the CORE
    // offers the star itself, and the socket must never appear while you are on the core.
    juce::Point<float> cursorPos;
    bool cursorInside { false };
    bool hoverOnRim { false };

    std::optional<InvisTheme> customTheme;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisConstellation)
};

} // namespace invis::ui
