package cframegraph

import (
	callocator "github.com/jurgen-kluft/callocator/package"
	cbase "github.com/jurgen-kluft/cbase/package"
	denv "github.com/jurgen-kluft/ccode/denv"
	cunittest "github.com/jurgen-kluft/cunittest/package"
)

// GetPackage returns the package object of 'cframegraph'
func GetPackage() *denv.Package {
	// Dependencies
	cunittestpkg := cunittest.GetPackage()
	cbasepkg := cbase.GetPackage()
	callocatorpkg := callocator.GetPackage()

	// The main (cframegraph) package
	mainpkg := denv.NewPackage("cframegraph")
	mainpkg.AddPackage(cunittestpkg)
	mainpkg.AddPackage(cbasepkg)
	mainpkg.AddPackage(callocatorpkg)

	// 'cframegraph' library
	mainlib := denv.SetupCppLibProject("cframegraph", "github.com\\jurgen-kluft\\cframegraph")
	mainlib.AddDependencies(cbasepkg.GetMainLib()...)
	mainlib.AddDependencies(callocatorpkg.GetMainLib()...)

	// 'cframegraph' unittest project
	maintest := denv.SetupDefaultCppTestProject("cframegraph"+"_test", "github.com\\jurgen-kluft\\cframegraph")
	maintest.AddDependencies(cunittestpkg.GetMainLib()...)
	maintest.Dependencies = append(maintest.Dependencies, mainlib)

	mainpkg.AddMainLib(mainlib)
	mainpkg.AddUnittest(maintest)
	return mainpkg
}
