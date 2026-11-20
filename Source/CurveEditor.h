#pragma once

#include "Engine.h"

class CurveEditor final : public juce::Component, private juce::AudioProcessorParameter::Listener
{
public:
    explicit CurveEditor(Engine& processor) : engine(processor)
    {
        for (auto* control : std::initializer_list<juce::Component*>{
                 &selector, &target, &search, &shape, &enabled, &minimum, &maximum, &length, &lastTouched})
            addAndMakeVisible(control);

        for (int i = 0; i < Engine::laneCount; ++i)
        {
            selector.addItem("Courbe " + juce::String(i + 1), i + 1);
            applyShape(lanes[i], 1);
        }
        for (const auto* name : {"Sinus", "Montée", "Descente", "Triangle", "Palier"})
            shape.addItem(name, shape.getNumItems() + 1);

        target.setTextWhenNothingSelected("Destination VST3");
        search.setTextToShowWhenEmpty("Filtrer les paramètres", juce::Colours::grey);
        search.onTextChange = [this] { refreshTargets(); };
        minimum.setRange(0, 100, 1);
        maximum.setRange(0, 100, 1);
        length.setRange(.25, 32, .25);
        for (auto* slider : {&minimum, &maximum, &length})
            slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 76, 24);
        minimum.setTextValueSuffix(" %");
        maximum.setTextValueSuffix(" %");
        length.setTextValueSuffix(" mes.");

        lastTouched.onClick = [this]
        {
            const auto parameter = touched.load();
            if (parameter < 0)
                return;
            search.clear();
            refreshTargets();
            if (target.isItemEnabled(parameter + 2))
                target.setSelectedId(parameter + 2, juce::sendNotificationSync);
        };
        selector.onChange = [this] { showLane(); };
        target.onChange = [this]
        {
            lane().target = target.getSelectedId() - 2;
            publish();
            refreshTargets();
        };
        enabled.onClick = [this] { lane().enabled = enabled.getToggleState(); publish(); };
        minimum.onValueChange = [this]
        {
            maximum.setValue(juce::jmax(maximum.getValue(), minimum.getValue()), juce::dontSendNotification);
            updateRange();
        };
        maximum.onValueChange = [this]
        {
            minimum.setValue(juce::jmin(maximum.getValue(), minimum.getValue()), juce::dontSendNotification);
            updateRange();
        };
        length.onValueChange = [this] { lane().bars = length.getValue(); publish(); };
        shape.onChange = [this] { applyShape(lane(), shape.getSelectedId()); publish(); };
        selector.setSelectedId(1, juce::dontSendNotification);
        showLane();
    }

    ~CurveEditor() override { detach(); }

    void detach()
    {
        if (listeningTo != nullptr)
            for (auto* parameter : listeningTo->getParameters())
                parameter->removeListener(this);
        listeningTo = nullptr;
        touched.store(-1);
    }

    void instrumentChanged()
    {
        for (int i = 0; i < Engine::laneCount; ++i)
        {
            lanes[i].target = -1;
            engine.setLane(i, lanes[i]);
        }
        listeningTo = engine.get();
        if (listeningTo != nullptr)
            for (auto* parameter : listeningTo->getParameters())
                parameter->addListener(this);
        search.clear();
        refreshTargets();
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(0xff1b2435));
        g.fillRoundedRectangle(graph.toFloat(), 6);
        g.setColour(juce::Colour(0xff364459));
        for (int i = 1; i < 8; ++i)
            g.drawVerticalLine(graph.getX() + graph.getWidth() * i / 8,
                               static_cast<float>(graph.getY()), static_cast<float>(graph.getBottom()));

        juce::Path path;
        const auto& values = lane().points;
        for (int i = 0; i < Engine::pointCount; ++i)
        {
            const auto x = graph.getX() + i * (graph.getWidth() - 1.0f) / (Engine::pointCount - 1);
            const auto y = graph.getBottom() - 1 - values[i] * (graph.getHeight() - 1);
            if (i == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }
        g.setColour(juce::Colour(0xff55d7bd));
        g.strokePath(path, juce::PathStrokeType(2));
        g.setColour(juce::Colours::white);
        const auto labelY = getHeight() - 52;
        g.drawText("Minimum", minimum.getX(), labelY, minimum.getWidth(), 18, juce::Justification::centredLeft);
        g.drawText("Maximum", maximum.getX(), labelY, maximum.getWidth(), 18, juce::Justification::centredLeft);
        g.drawText("Durée de la courbe", length.getX(), labelY, length.getWidth(), 18, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto top = area.removeFromTop(34);
        lastTouched.setBounds(top.removeFromRight(120).reduced(3));
        selector.setBounds(top.removeFromLeft(112).reduced(3));
        enabled.setBounds(top.removeFromLeft(82).reduced(3));
        shape.setBounds(top.removeFromLeft(110).reduced(3));
        search.setBounds(top.removeFromLeft(top.getWidth() / 2).reduced(3));
        target.setBounds(top.reduced(3));
        auto bottom = area.removeFromBottom(52).withTrimmedTop(18);
        const auto width = bottom.getWidth() / 3;
        minimum.setBounds(bottom.removeFromLeft(width).reduced(3));
        maximum.setBounds(bottom.removeFromLeft(width).reduced(3));
        length.setBounds(bottom.reduced(3));
        graph = area.reduced(3);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        drawing = graph.contains(e.getPosition());
        if (drawing)
        {
            previous = pointAt(e.position);
            drawTo(previous);
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (drawing)
            drawTo(pointAt(e.position));
    }

    void mouseUp(const juce::MouseEvent&) override { drawing = false; }

private:
    Engine& engine;
    std::array<Engine::Lane, Engine::laneCount> lanes;
    juce::ComboBox selector, target, shape;
    juce::TextEditor search;
    juce::TextButton lastTouched{"Dernier potard"};
    juce::AudioPluginInstance* listeningTo = nullptr;
    std::atomic<int> touched{-1};
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int parameter, bool starting) override
    {
        if (starting)
            touched.store(parameter);
    }
    juce::ToggleButton enabled{"Active"};
    juce::Slider minimum, maximum, length;
    juce::Rectangle<int> graph;
    juce::Point<float> previous;
    bool drawing = false;

    int index() const { return juce::jmax(0, selector.getSelectedId() - 1); }
    Engine::Lane& lane() { return lanes[index()]; }
    const Engine::Lane& lane() const { return lanes[index()]; }

    void publish()
    {
        engine.setLane(index(), lane());
        repaint(graph);
    }

    void updateRange()
    {
        lane().minimum = static_cast<float>(minimum.getValue() / 100);
        lane().maximum = static_cast<float>(maximum.getValue() / 100);
        publish();
    }

    void showLane()
    {
        enabled.setToggleState(lane().enabled, juce::dontSendNotification);
        minimum.setValue(lane().minimum * 100, juce::dontSendNotification);
        maximum.setValue(lane().maximum * 100, juce::dontSendNotification);
        length.setValue(lane().bars, juce::dontSendNotification);
        shape.setTextWhenNothingSelected("Forme");
        shape.setSelectedId(0, juce::dontSendNotification);
        refreshTargets();
        repaint();
    }

    void refreshTargets()
    {
        target.clear(juce::dontSendNotification);
        target.addItem("Aucune destination", 1);
        if (auto* instrument = engine.get())
        {
            const auto& params = instrument->getParameters();
            for (int i = 0; i < params.size(); ++i)
            {
                if (!params[i]->isAutomatable())
                    continue;
                const auto name = params[i]->getName(100) + " [" + juce::String(i + 1) + "]";
                if (i != lane().target && !name.containsIgnoreCase(search.getText()))
                    continue;
                target.addItem(name, i + 2);
                for (int other = 0; other < Engine::laneCount; ++other)
                    if (other != index() && lanes[other].target == i)
                        target.setItemEnabled(i + 2, false);
            }
        }
        target.setSelectedId(lane().target + 2, juce::dontSendNotification);
    }

    static void applyShape(Engine::Lane& current, int id)
    {
        for (int i = 0; i < Engine::pointCount; ++i)
        {
            const auto phase = static_cast<float>(i) / (Engine::pointCount - 1);
            switch (id)
            {
                case 1: current.points[i] = .5f - .5f * std::cos(juce::MathConstants<float>::twoPi * phase); break;
                case 2: current.points[i] = phase; break;
                case 3: current.points[i] = 1 - phase; break;
                case 4: current.points[i] = 1 - std::abs(2 * phase - 1); break;
                case 5: current.points[i] = std::floor(phase * 8) / 8; break;
                default: break;
            }
        }
    }

    juce::Point<float> pointAt(juce::Point<float> point) const
    {
        return {juce::jlimit(0.0f, float(Engine::pointCount - 1),
                             (point.x - graph.getX()) * (Engine::pointCount - 1) / juce::jmax(1, graph.getWidth() - 1)),
                juce::jlimit(0.0f, 1.0f, (graph.getBottom() - 1 - point.y) / juce::jmax(1, graph.getHeight() - 1))};
    }

    void drawTo(juce::Point<float> point)
    {
        const auto from = juce::roundToInt(previous.x);
        const auto to = juce::roundToInt(point.x);
        for (int i = juce::jmin(from, to); i <= juce::jmax(from, to); ++i)
            lane().points[i] = from == to ? point.y :
                juce::jmap(static_cast<float>(i - from) / (to - from), previous.y, point.y);
        previous = point;
        shape.setSelectedId(0, juce::dontSendNotification);
        publish();
    }
};
