/*
 * Copyright (C) 2014-2016 Olzhas Rakhimov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/// @file event.h
/// Contains event classes for fault trees.

#ifndef SCRAM_SRC_EVENT_H_
#define SCRAM_SRC_EVENT_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "element.h"
#include "error.h"
#include "expression.h"

namespace scram {

/// @class Event
/// Abstract base class for general fault tree events.
class Event : public Element, public Role {
 public:
  /// Constructs a fault tree event with a specific id.
  /// It is assumed that names
  /// and other strings do not have
  /// leading and trailing whitespace characters.
  ///
  /// @param[in] name  The identifying name with caps preserved.
  /// @param[in] base_path  The series of containers to get this event.
  /// @param[in] is_public  Whether or not the event is public.
  explicit Event(const std::string& name, const std::string& base_path = "",
                 bool is_public = true);

  Event(const Event&) = delete;
  Event& operator=(const Event&) = delete;

  virtual ~Event() = 0;  ///< Abstract class.

  /// @returns The id that is set upon the construction of this event.
  const std::string& id() const { return id_; }

  /// @returns The original name with capitalizations.
  const std::string& name() const { return name_; }

  /// @returns True if this node is orphan.
  bool orphan() const { return orphan_; }

  /// Sets the orphan state.
  ///
  /// @param[in] state  True if this event is not used anywhere.
  void orphan(bool state) { orphan_ = state; }

 private:
  std::string id_;  ///< Id name of a event. It is in lower case.
  std::string name_;  ///< Original name with capitalizations preserved.
  bool orphan_;  ///< Indication of an orphan node.
};

/// @class PrimaryEvent
/// This is an abstract base class for events
/// that can cause failures.
/// This class represents Base, House, Undeveloped, and other events.
class PrimaryEvent : public Event {
 public:
  using Event::Event;  // Construction with unique identification.
  virtual ~PrimaryEvent() = 0;  ///< Abstract class.

  /// @returns A flag indicating if the event's expression is set.
  bool has_expression() const { return has_expression_; }

 protected:
  /// Sets indication for existence of an expression.
  ///
  /// @param[in] flag  true if the expression is defined.
  void has_expression(bool flag) { has_expression_ = flag; }

 private:
  /// Flag to notify that expression for the event is defined.
  bool has_expression_ = false;
};

/// @class HouseEvent
/// Representation of a house event in a fault tree.
class HouseEvent : public PrimaryEvent {
 public:
  using PrimaryEvent::PrimaryEvent;  // Construction with unique identification.

  /// Sets the state for House event.
  ///
  /// @param[in] constant  False or True for the state of this house event.
  void state(bool constant) {
    PrimaryEvent::has_expression(true);
    state_ = constant;
  }

  /// @returns The true or false state of this house event.
  bool state() const { return state_; }

 private:
  /// Represents the state of the house event.
  /// Implies On or Off for True or False values of the probability.
  bool state_ = false;
};

class Gate;
using GatePtr = std::shared_ptr<Gate>;  ///< Shared gates in models.

/// @class BasicEvent
/// Representation of a basic event in a fault tree.
class BasicEvent : public PrimaryEvent {
 public:
  using PrimaryEvent::PrimaryEvent;  // Construction with unique identification.

  virtual ~BasicEvent() = default;

  /// Sets the expression of this basic event.
  ///
  /// @param[in] expression  The expression to describe this event.
  void expression(const ExpressionPtr& expression) {
    assert(!expression_);
    PrimaryEvent::has_expression(true);
    expression_ = expression;
  }

  /// @returns The mean probability of this basic event.
  ///
  /// @note The user of this function should make sure
  ///       that the returned value is acceptable for calculations.
  ///
  /// @warning Undefined behavior if the expression is not set.
  double p() const noexcept {
    assert(expression_);
    return expression_->Mean();
  }

  /// Samples probability value from its probability distribution.
  ///
  /// @returns Sampled value.
  ///
  /// @note The user of this function should make sure
  ///       that the returned value is acceptable for calculations.
  ///
  /// @warning Undefined behavior if the expression is not set.
  double SampleProbability() noexcept {
    assert(expression_);
    return expression_->Sample();
  }

  /// Resets the sampling.
  void Reset() noexcept { expression_->Reset(); }

  /// @returns Indication if this event does not have uncertainty.
  bool IsConstant() noexcept { return expression_->IsConstant(); }

  /// Validates the probability expressions for the primary event.
  ///
  /// @throws ValidationError  The expression for the basic event is invalid.
  void Validate() {
    if (expression_->Min() < 0 || expression_->Max() > 1) {
      throw ValidationError("Expression value is invalid.");
    }
  }

  /// Indicates if this basic event has been set to be in a CCF group.
  ///
  /// @returns true if in a CCF group.
  /// @returns false otherwise.
  bool HasCcf() const { return ccf_gate_ ? true : false; }

  /// @returns CCF group gate representing this basic event.
  const GatePtr& ccf_gate() const {
    assert(ccf_gate_);
    return ccf_gate_;
  }

  /// Sets the common cause failure group gate
  /// that can represent this basic event
  /// in analysis with common cause information.
  /// This information is expected to be provided by
  /// CCF group application.
  ///
  /// @param[in] gate  CCF group gate.
  void ccf_gate(const GatePtr& gate) {
    assert(!ccf_gate_);
    ccf_gate_ = gate;
  }

 private:
  /// Expression that describes this basic event
  /// and provides numerical values for probability calculations.
  ExpressionPtr expression_;

  /// If this basic event is in a common cause group,
  /// CCF gate can serve as a replacement for the basic event
  /// for common cause analysis.
  GatePtr ccf_gate_;
};

class CcfGroup;

/// @class CcfEvent
/// A basic event that represents a multiple failure of
/// a group of events due to a common cause.
/// This event is generated out of a common cause group.
/// This class is a helper to report correctly the CCF events.
class CcfEvent : public BasicEvent {
 public:
  /// Constructs CCF event with specific name
  /// that is used for internal purposes.
  /// This name is formatted by the CcfGroup.
  /// The creator CCF group
  /// and names of the member events of this specific CCF event
  /// are saved for reporting.
  ///
  /// @param[in] name  The identifying name of this CCF event.
  /// @param[in] ccf_group  The CCF group that created this event.
  /// @param[in] member_names  The names of members that this CCF event
  ///                          represents as multiple failure.
  CcfEvent(const std::string& name, const CcfGroup* ccf_group,
           const std::vector<std::string>& member_names);

  /// @returns Pointer to the CCF group that created this CCF event.
  const CcfGroup* ccf_group() const { return ccf_group_; }

  /// @returns Original names of members of this CCF event.
  const std::vector<std::string>& member_names() const { return member_names_; }

 private:
  const CcfGroup* ccf_group_;  ///< Pointer to the CCF group.
  /// Original names of basic events in this CCF event.
  std::vector<std::string> member_names_;
};

using EventPtr = std::shared_ptr<Event>;  ///< Base shared pointer for events.
using PrimaryEventPtr = std::shared_ptr<PrimaryEvent>;  ///< Base shared ptr.
using HouseEventPtr = std::shared_ptr<HouseEvent>;  ///< Shared house events.
using BasicEventPtr = std::shared_ptr<BasicEvent>;  ///< Shared basic events.

class Formula;  // To describe a gate's formula.
using FormulaPtr = std::unique_ptr<Formula>;  ///< Non-shared gate formulas.

/// @class Gate
/// A representation of a gate in a fault tree.
class Gate : public Event {
 public:
  using Event::Event;  // Construction with unique identification.

  /// @returns The formula of this gate.
  const FormulaPtr& formula() const { return formula_; }

  /// Sets the formula of this gate.
  ///
  /// @param[in] formula  Boolean formula of this gate.
  void formula(FormulaPtr formula) {
    assert(!formula_);
    formula_ = std::move(formula);
  }

  /// This function is for cycle detection.
  ///
  /// @returns The connector between gates.
  Formula* connector() const { return formula_.get(); }

  /// Checks if a gate is initialized correctly.
  ///
  /// @throws ValidationError  Errors in the gate's logic or setup.
  void Validate();

  /// @returns The mark of this gate node.
  /// @returns Empty string for no mark.
  const std::string& mark() const { return mark_; }

  /// Sets the mark for this gate node.
  void mark(const std::string& new_mark) { mark_ = new_mark; }

 private:
  FormulaPtr formula_;  ///< Boolean formula of this gate.
  std::string mark_ = "";  ///< The mark for traversal or toposort.
};

/// @class Formula
/// Boolean formula with operators and arguments.
/// Formulas are not expected to be shared.
class Formula {
 public:
  /// Constructs a formula.
  ///
  /// @param[in] type  The logical operator for this Boolean formula.
  explicit Formula(const std::string& type);

  Formula(const Formula&) = delete;
  Formula& operator=(const Formula&) = delete;

  /// @returns The type of this formula.
  ///
  /// @throws LogicError  The gate is not yet assigned.
  const std::string& type() const { return type_; }

  /// @returns The vote number if and only if the operator is ATLEAST.
  ///
  /// @throws LogicError  The vote number is not yet assigned.
  int vote_number() const;

  /// Sets the vote number only for an ATLEAST formula.
  ///
  /// @param[in] number  The vote number.
  ///
  /// @throws InvalidArgument  The vote number is invalid.
  /// @throws LogicError  The vote number is assigned illegally.
  ///
  /// @note (Children number > vote number) should be checked
  ///       outside of this class.
  void vote_number(int number);

  /// @returns The arguments of this formula of specific type.
  /// @{
  const std::map<std::string, EventPtr>& event_args() const {
    return event_args_;
  }
  const std::vector<HouseEventPtr>& house_event_args() const {
    return house_event_args_;
  }
  const std::vector<BasicEventPtr>& basic_event_args() const {
    return basic_event_args_;
  }
  const std::vector<GatePtr>& gate_args() const { return gate_args_; }
  const std::vector<FormulaPtr>& formula_args() const { return formula_args_; }
  /// @}

  /// @returns The number of arguments.
  int num_args() const { return event_args_.size() + formula_args_.size(); }

  /// Adds an event into the arguments list.
  ///
  /// @param[in] event  A pointer to an argument event.
  ///
  /// @throws DuplicateArgumentError  The argument event is duplicate.
  ///
  /// @{
  void AddArgument(const HouseEventPtr& event) {
    AddArgument(event, &house_event_args_);
  }
  void AddArgument(const BasicEventPtr& event) {
    AddArgument(event, &basic_event_args_);
  }
  void AddArgument(const GatePtr& event) {
    AddArgument(event, &gate_args_);
  }
  /// @}

  /// Adds a formula into the arguments list.
  /// Formulas are unique.
  ///
  /// @param[in] formula  A pointer to an argument formula.
  void AddArgument(FormulaPtr formula);

  /// Checks if a formula is initialized correctly with the number of arguments.
  ///
  /// @throws ValidationError  Problems with the operator or arguments.
  void Validate();

  /// @returns Gates as nodes.
  const std::vector<Gate*>& nodes() {
    if (gather_) Formula::GatherNodesAndConnectors();
    return nodes_;
  }

  /// @returns Formulae as connectors.
  const std::vector<Formula*>& connectors() {
    if (gather_) Formula::GatherNodesAndConnectors();
    return connectors_;
  }

 private:
  /// Formula types that require two or more arguments.
  static const std::set<std::string> kTwoOrMore_;
  /// Formula types that require exactly one argument.
  static const std::set<std::string> kSingle_;

  /// Handles addition of an event to the formula.
  ///
  /// @tparam Ptr  Shared pointer type to the event.
  ///
  /// @param[in] event  Pointer to the event.
  /// @param[in,out] container  The final destination to save the event.
  ///
  /// @throws DuplicateArgumentError  The argument even tis duplicate.
  template<class Ptr>
  void AddArgument(const Ptr& event, std::vector<Ptr>* container) {
    if (event_args_.count(event->id()))
      throw DuplicateArgumentError("Duplicate argument " + event->name());
    event_args_.emplace(event->id(), event);
    container->emplace_back(event);
  }

  /// Gathers nodes and connectors from arguments of the gate.
  void GatherNodesAndConnectors();

  std::string type_;  ///< Logical operator.
  int vote_number_;  ///< Vote number for ATLEAST operator.
  std::map<std::string, EventPtr> event_args_;  ///< All event arguments.
  std::vector<HouseEventPtr> house_event_args_;  ///< House event arguments.
  std::vector<BasicEventPtr> basic_event_args_;  ///< Basic event arguments.
  std::vector<GatePtr> gate_args_;  ///< Arguments that are gates.
  /// Arguments that are formulas
  /// if this formula is nested.
  std::vector<FormulaPtr> formula_args_;
  std::vector<Gate*> nodes_;  ///< Gate arguments as nodes.
  std::vector<Formula*> connectors_;  ///< Formulae as connectors.
  bool gather_;  ///< A flag to gather nodes and connectors.
};

}  // namespace scram

#endif  // SCRAM_SRC_EVENT_H_
